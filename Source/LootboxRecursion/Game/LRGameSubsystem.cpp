#include "Game/LRGameSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/LRSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "LootboxRecursion.h"
#include "Misc/DateTime.h"

const FString ULRGameSubsystem::SaveSlotName = TEXT("LootboxRecursion");

namespace
{
	constexpr double AutosaveIntervalSeconds = 30.0;
	constexpr int32 MaxFocusDistance = 1000000;
}

ULRGameSubsystem* ULRGameSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<ULRGameSubsystem>() : nullptr;
}

void ULRGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Rails: config/initializers/app_data.rb loading the YAML.
	FLRGameData Data;
	DataErrors.Reset();
	const FString DataDirectory = FLRGameData::GetDefaultDataDirectory();
	if (!FLRGameData::LoadFromDirectory(DataDirectory, Data, DataErrors))
	{
		for (const FString& Error : DataErrors)
		{
			UE_LOG(LogLootbox, Error, TEXT("Game data: %s"), *Error);
		}
	}
	UE_LOG(LogLootbox, Log, TEXT("Loaded game data from %s: %d items, %d recipes, %d loot tables, %d actions"),
		*DataDirectory, Data.Items.Num(), Data.Recipes.Num(), Data.LootTables.Num(), Data.Actions.Num());

	const int32 Seed = static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff);
	Simulation = MakeUnique<FLRSimulation>(Data, Seed);
	Simulation->OnInventoryChanged.AddUObject(this, &ULRGameSubsystem::HandleSimInventoryChanged);
	Simulation->OnWorldChanged.AddUObject(this, &ULRGameSubsystem::HandleSimWorldChanged);
	Simulation->OnActionCompleted.AddUObject(this, &ULRGameSubsystem::HandleSimActionCompleted);

	LoadGame();

	// FTSTicker calls us once per engine frame, independent of any world or actor.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULRGameSubsystem::Tick));
}

void ULRGameSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	SaveNow();

	if (Simulation)
	{
		Simulation->OnInventoryChanged.RemoveAll(this);
		Simulation->OnWorldChanged.RemoveAll(this);
		Simulation->OnActionCompleted.RemoveAll(this);
		Simulation.Reset();
	}
	Super::Deinitialize();
}

bool ULRGameSubsystem::Tick(float DeltaSeconds)
{
	if (Simulation)
	{
		Simulation->Advance(static_cast<double>(DeltaSeconds) * TimeScale);
	}

	SecondsSinceAutosave += DeltaSeconds;
	if (SecondsSinceAutosave >= AutosaveIntervalSeconds)
	{
		SecondsSinceAutosave = 0.0;
		SaveNow();
	}
	return true; // keep ticking
}

// ---- Persistence ----------------------------------------------------------------------

bool ULRGameSubsystem::SaveNow()
{
	if (!Simulation || !DataErrors.IsEmpty())
	{
		// Never overwrite a good save while running on broken data.
		return false;
	}
	ULRSaveGame* Save = Cast<ULRSaveGame>(UGameplayStatics::CreateSaveGameObject(ULRSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->Data = Simulation->Save();
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool ULRGameSubsystem::LoadGame()
{
	if (!Simulation || !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return false;
	}
	const ULRSaveGame* Save = Cast<ULRSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save || !Simulation->Load(Save->Data))
	{
		UE_LOG(LogLootbox, Warning, TEXT("Save slot '%s' could not be loaded (incompatible version?) - starting fresh"), *SaveSlotName);
		return false;
	}
	UE_LOG(LogLootbox, Log, TEXT("Loaded save slot '%s' (sim time %.0fs)"), *SaveSlotName, Simulation->GetNow());
	return true;
}

void ULRGameSubsystem::ResetGame()
{
	if (!Simulation)
	{
		return;
	}
	Simulation->Reset(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));
	SelectedSlot = INDEX_NONE;
	bHasSelectedCell = false;
	FocusCoordinate = 0;
	OnSelectionChanged.Broadcast();
	SaveNow();
}

// ---- Data -----------------------------------------------------------------------------

bool ULRGameSubsystem::GetItemDef(FName Item, FLRItemDef& OutDef) const
{
	const FLRItemDef* Def = Simulation ? Simulation->GetData().FindItem(Item) : nullptr;
	if (Def)
	{
		OutDef = *Def;
	}
	return Def != nullptr;
}

// ---- Commands -------------------------------------------------------------------------

FLRActionResult ULRGameSubsystem::RequestAction(const FLRActionRequest& Request)
{
	return Simulation ? Simulation->RequestAction(Request) : FLRActionResult();
}

FLRActionResult ULRGameSubsystem::RequestSimpleAction(FName Action)
{
	return RequestAction(FLRActionRequest::Make(Action));
}

FLRActionResult ULRGameSubsystem::Craft(FName RecipeId)
{
	FLRActionRequest Request = FLRActionRequest::Make(LRNames::Craft);
	Request.Choice = RecipeId;
	return RequestAction(Request);
}

FLRActionResult ULRGameSubsystem::RequestActionWithSelection(FName Action)
{
	FLRActionRequest Request = FLRActionRequest::Make(Action);
	Request.Slot = SelectedSlot;
	if (bHasSelectedCell)
	{
		Request.Coordinate = SelectedCell;
		Request.bHasCoordinate = true;
	}
	return RequestAction(Request);
}

bool ULRGameSubsystem::GiveItem(FName Item, int32 Count)
{
	return Simulation && Simulation->GiveItem(Item, Count);
}

// ---- Queries --------------------------------------------------------------------------

FLRActionStatus ULRGameSubsystem::GetActionStatus(FName Action) const
{
	return Simulation ? Simulation->GetActionStatus(Action) : FLRActionStatus();
}

TArray<FLRInventorySlot> ULRGameSubsystem::GetInventory() const
{
	return Simulation ? Simulation->GetInventory() : TArray<FLRInventorySlot>();
}

int32 ULRGameSubsystem::CountItem(FName Item) const
{
	return Simulation ? Simulation->CountItem(Item) : 0;
}

bool ULRGameSubsystem::GetPlacedAt(int32 Coordinate, FLRPlacedEntity& OutEntity) const
{
	const FLRPlacedEntity* Entity = Simulation ? Simulation->FindPlaced(Coordinate) : nullptr;
	if (Entity)
	{
		OutEntity = *Entity;
	}
	return Entity != nullptr;
}

double ULRGameSubsystem::GetSimTime() const
{
	return Simulation ? Simulation->GetNow() : 0.0;
}

// ---- Selection ------------------------------------------------------------------------

void ULRGameSubsystem::SelectSlot(int32 SlotIndex)
{
	// Clicking the selected slot again deselects it (same as the Vue store).
	SelectedSlot = (SlotIndex == SelectedSlot) ? INDEX_NONE : SlotIndex;
	OnSelectionChanged.Broadcast();
}

void ULRGameSubsystem::SelectCell(int32 Coordinate)
{
	if (bHasSelectedCell && SelectedCell == Coordinate)
	{
		bHasSelectedCell = false;
	}
	else
	{
		SelectedCell = Coordinate;
		bHasSelectedCell = true;
	}
	OnSelectionChanged.Broadcast();
}

void ULRGameSubsystem::ClearCellSelection()
{
	bHasSelectedCell = false;
	OnSelectionChanged.Broadcast();
}

bool ULRGameSubsystem::GetSelectedCell(int32& OutCoordinate) const
{
	OutCoordinate = SelectedCell;
	return bHasSelectedCell;
}

void ULRGameSubsystem::SetFocusCoordinate(int32 Coordinate)
{
	FocusCoordinate = FMath::Clamp(Coordinate, -MaxFocusDistance, MaxFocusDistance);
}

void ULRGameSubsystem::FocusHome()
{
	int32 Home = 0;
	bool bFound = false;
	if (Simulation)
	{
		for (const TPair<int32, FLRPlacedEntity>& Pair : Simulation->GetPlaced())
		{
			if (!bFound || Pair.Key < Home)
			{
				Home = Pair.Key;
				bFound = true;
			}
		}
	}
	SetFocusCoordinate(Home);
}

// ---- Simulation event forwarding ------------------------------------------------------

void ULRGameSubsystem::HandleSimInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

void ULRGameSubsystem::HandleSimWorldChanged()
{
	OnWorldChanged.Broadcast();
}

void ULRGameSubsystem::HandleSimActionCompleted(const FLRActionResult& Result)
{
	UE_LOG(LogLootbox, Log, TEXT("[%s] %s"), *Result.Action.ToString(), *Result.Message);
	OnActionCompleted.Broadcast(Result);
}
