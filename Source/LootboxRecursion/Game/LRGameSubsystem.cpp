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
	constexpr int32 MaxBuildLayer = 256;
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
	FocusOnCell(FIntVector::ZeroValue);
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
		Request.Cell = SelectedCell;
		Request.bHasCell = true;
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

bool ULRGameSubsystem::GetPlacedAt(FIntVector Cell, FLRPlacedEntity& OutEntity) const
{
	const FLRPlacedEntity* Entity = Simulation ? Simulation->FindPlaced(Cell) : nullptr;
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

FString ULRGameSubsystem::GetEpochName() const
{
	const FLREpochDef* Epoch = Simulation ? Simulation->GetEpoch() : nullptr;
	return Epoch ? Epoch->Name : FString();
}

FString ULRGameSubsystem::GetCosmicTimeText() const
{
	return Simulation ? FLRSimulation::FormatCosmicTime(Simulation->GetCosmicTime()) : FString();
}

float ULRGameSubsystem::GetHostMass() const
{
	return Simulation ? static_cast<float>(Simulation->GetHostMass()) : 1.f;
}

float ULRGameSubsystem::GetPlasmaOpacity() const
{
	return Simulation ? Simulation->GetPlasmaOpacity() : 0.f;
}

// ---- Selection ------------------------------------------------------------------------

void ULRGameSubsystem::SelectSlot(int32 SlotIndex)
{
	// Clicking the selected slot again deselects it (same as the Vue store).
	SelectedSlot = (SlotIndex == SelectedSlot) ? INDEX_NONE : SlotIndex;
	OnSelectionChanged.Broadcast();
}

void ULRGameSubsystem::SelectCell(FIntVector Cell, bool bToggle)
{
	if (bToggle && bHasSelectedCell && SelectedCell == Cell)
	{
		bHasSelectedCell = false;
	}
	else
	{
		SelectedCell = Cell;
		bHasSelectedCell = true;
	}
	OnSelectionChanged.Broadcast();
}

void ULRGameSubsystem::ClearCellSelection()
{
	bHasSelectedCell = false;
	OnSelectionChanged.Broadcast();
}

bool ULRGameSubsystem::GetSelectedCell(FIntVector& OutCell) const
{
	OutCell = SelectedCell;
	return bHasSelectedCell;
}

bool ULRGameSubsystem::GetHoveredCell(FIntVector& OutCell) const
{
	OutCell = HoveredCell;
	return bHasHoveredCell;
}

void ULRGameSubsystem::SetBuildLayer(int32 Layer)
{
	BuildLayer = FMath::Clamp(Layer, -MaxBuildLayer, MaxBuildLayer);
}

void ULRGameSubsystem::FocusOnCell(FIntVector Cell)
{
	FocusRequestCell = Cell;
	++FocusRequestSerial;
	SetBuildLayer(Cell.Z);
}

TArray<FLRPlacedEntity> ULRGameSubsystem::GetPlacedSorted() const
{
	TArray<FLRPlacedEntity> Out;
	if (Simulation)
	{
		Simulation->GetPlaced().GenerateValueArray(Out);
	}
	Out.Sort([](const FLRPlacedEntity& A, const FLRPlacedEntity& B)
	{
		if (A.Cell.Z != B.Cell.Z) { return A.Cell.Z < B.Cell.Z; }
		if (A.Cell.Y != B.Cell.Y) { return A.Cell.Y < B.Cell.Y; }
		return A.Cell.X < B.Cell.X;
	});
	return Out;
}

void ULRGameSubsystem::FocusHome()
{
	const TArray<FLRPlacedEntity> Sorted = GetPlacedSorted();
	FocusOnCell(Sorted.IsEmpty() ? FIntVector::ZeroValue : Sorted[0].Cell);
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
