#include "Game/LRGameSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/LRSaveGame.h"
#include "Game/LRUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "LootboxRecursion.h"
#include "Misc/DateTime.h"

const FString ULRGameSubsystem::SaveSlotName = TEXT("LootboxRecursion");
const FString ULRGameSubsystem::SaveIndexSlotName = TEXT("SaveIndex");

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
	Simulation->OnMatterChanged.AddUObject(this, &ULRGameSubsystem::HandleSimMatterChanged);
	Simulation->OnWorldChanged.AddUObject(this, &ULRGameSubsystem::HandleSimWorldChanged);
	Simulation->OnActionCompleted.AddUObject(this, &ULRGameSubsystem::HandleSimActionCompleted);

	UserSettings = ULRUserSettings::LoadOrCreate();
	LoadGame(SaveSlotName);

	// FTSTicker calls us once per engine frame, independent of any world or actor.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULRGameSubsystem::Tick));
}

void ULRGameSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	SaveNow();

	if (Simulation)
	{
		Simulation->OnMatterChanged.RemoveAll(this);
		Simulation->OnWorldChanged.RemoveAll(this);
		Simulation->OnActionCompleted.RemoveAll(this);
		Simulation.Reset();
	}
	Super::Deinitialize();
}

bool ULRGameSubsystem::Tick(float DeltaSeconds)
{
	if (bPaused)
	{
		return true; // the menu is open: nothing moves
	}
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
	return WriteSlot(SaveSlotName, TEXT("Autosave"), /*bAutosave*/ true);
}

bool ULRGameSubsystem::WriteSlot(const FString& SlotName, const FString& DisplayName, bool bAutosave)
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
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
	{
		return false;
	}

	// List it in the index, with a line about the game in it.
	ULRSaveIndex* Index = LoadSaveIndex();
	if (!Index)
	{
		return true;
	}
	FLRSaveSlotInfo* Info = Index->Slots.FindByPredicate([&SlotName](const FLRSaveSlotInfo& Each) { return Each.SlotName == SlotName; });
	if (!Info)
	{
		Info = &Index->Slots.AddDefaulted_GetRef();
		Info->SlotName = SlotName;
	}
	Info->DisplayName = DisplayName;
	Info->bAutosave = bAutosave;
	Info->SavedAt = FDateTime::Now();
	const FString Epoch = GetEpochName();
	Info->Summary = FString::Printf(TEXT("%s%shost %s, played %.0f min"), *Epoch, Epoch.IsEmpty() ? TEXT("") : TEXT(", "),
		*FLRSimulation::FormatMass(Simulation->GetHostMass()), Simulation->GetNow() / 60.0);
	UGameplayStatics::SaveGameToSlot(Index, SaveIndexSlotName, 0);
	return true;
}

ULRSaveIndex* ULRGameSubsystem::LoadSaveIndex() const
{
	if (UGameplayStatics::DoesSaveGameExist(SaveIndexSlotName, 0))
	{
		if (ULRSaveIndex* Index = Cast<ULRSaveIndex>(UGameplayStatics::LoadGameFromSlot(SaveIndexSlotName, 0)))
		{
			return Index;
		}
	}
	return Cast<ULRSaveIndex>(UGameplayStatics::CreateSaveGameObject(ULRSaveIndex::StaticClass()));
}

TArray<FLRSaveSlotInfo> ULRGameSubsystem::GetSaveSlots() const
{
	TArray<FLRSaveSlotInfo> Slots;
	if (const ULRSaveIndex* Index = LoadSaveIndex())
	{
		for (const FLRSaveSlotInfo& Info : Index->Slots)
		{
			if (UGameplayStatics::DoesSaveGameExist(Info.SlotName, 0))
			{
				Slots.Add(Info);
			}
		}
	}
	Slots.Sort([](const FLRSaveSlotInfo& A, const FLRSaveSlotInfo& B)
	{
		return A.bAutosave != B.bAutosave ? A.bAutosave : A.SavedAt > B.SavedAt;
	});
	return Slots;
}

bool ULRGameSubsystem::SaveToNewSlot(const FString& DisplayName)
{
	ULRSaveIndex* Index = LoadSaveIndex();
	if (!Index)
	{
		return false;
	}
	const FString SlotName = FString::Printf(TEXT("Save_%d"), Index->NextSlotNumber++);
	UGameplayStatics::SaveGameToSlot(Index, SaveIndexSlotName, 0); // claim the number
	const FString Name = DisplayName.TrimStartAndEnd();
	return WriteSlot(SlotName, Name.IsEmpty() ? SlotName.Replace(TEXT("_"), TEXT(" ")) : Name, /*bAutosave*/ false);
}

bool ULRGameSubsystem::SaveToSlot(const FString& SlotName)
{
	const TArray<FLRSaveSlotInfo> Slots = GetSaveSlots();
	const FLRSaveSlotInfo* Info = Slots.FindByPredicate([&SlotName](const FLRSaveSlotInfo& Each) { return Each.SlotName == SlotName; });
	return Info && WriteSlot(SlotName, Info->DisplayName, Info->bAutosave);
}

bool ULRGameSubsystem::LoadFromSlot(const FString& SlotName)
{
	if (!LoadGame(SlotName))
	{
		return false;
	}
	HandleGameReplaced();
	if (SlotName != SaveSlotName)
	{
		SaveNow(); // the loaded game is now the one that autosaves
	}
	return true;
}

bool ULRGameSubsystem::DeleteSlot(const FString& SlotName)
{
	if (SlotName == SaveSlotName)
	{
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	if (ULRSaveIndex* Index = LoadSaveIndex())
	{
		Index->Slots.RemoveAll([&SlotName](const FLRSaveSlotInfo& Each) { return Each.SlotName == SlotName; });
		UGameplayStatics::SaveGameToSlot(Index, SaveIndexSlotName, 0);
	}
	return true;
}

void ULRGameSubsystem::HandleGameReplaced()
{
	bHasSelectedCell = false;
	SecondsSinceAutosave = 0.0;
	FocusHome();
	OnSelectionChanged.Broadcast();
	OnWorldChanged.Broadcast();
	OnMatterChanged.Broadcast();
}

void ULRGameSubsystem::SaveUserSettings()
{
	if (UserSettings)
	{
		UserSettings->SaveToDisk();
	}
	OnSettingsChanged.Broadcast();
}

bool ULRGameSubsystem::LoadGame(const FString& SlotName)
{
	if (!Simulation || !UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}
	const ULRSaveGame* Save = Cast<ULRSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save || !Simulation->Load(Save->Data))
	{
		UE_LOG(LogLootbox, Warning, TEXT("Save slot '%s' could not be loaded (incompatible version?)"), *SlotName);
		return false;
	}
	UE_LOG(LogLootbox, Log, TEXT("Loaded save slot '%s' (sim time %.0fs)"), *SlotName, Simulation->GetNow());
	return true;
}

void ULRGameSubsystem::ResetGame()
{
	if (!Simulation)
	{
		return;
	}
	Simulation->Reset(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));
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
	if (bHasSelectedCell)
	{
		Request.Cell = SelectedCell;
		Request.bHasCell = true;
	}
	return RequestAction(Request);
}

FLRActionResult ULRGameSubsystem::RequestActionWithSelection(FName Action)
{
	FLRActionRequest Request = FLRActionRequest::Make(Action);
	if (bHasSelectedCell)
	{
		Request.Cell = SelectedCell;
		Request.bHasCell = true;
	}
	return RequestAction(Request);
}

bool ULRGameSubsystem::GiveMatter(FName Item, int32 Count)
{
	return Simulation && Simulation->GiveMatter(bHasSelectedCell ? SelectedCell : FIntVector::ZeroValue, Item, Count);
}

// ---- Queries --------------------------------------------------------------------------

FLRActionStatus ULRGameSubsystem::GetActionStatus(FName Action) const
{
	return Simulation ? Simulation->GetActionStatus(Action) : FLRActionStatus();
}

int32 ULRGameSubsystem::GetMatterAt(FIntVector Cell, FName Item) const
{
	return Simulation ? Simulation->GetMatter(Cell, Item) : 0;
}

int32 ULRGameSubsystem::GetTotalMatter(FName Item) const
{
	return Simulation ? Simulation->GetTotalMatter(Item) : 0;
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

double ULRGameSubsystem::GetHostMass() const
{
	return Simulation ? Simulation->GetHostMass() : 0.0;
}

void ULRGameSubsystem::SetInjectorTarget(double KgPerSecond)
{
	if (Simulation)
	{
		Simulation->SetInjectorTarget(KgPerSecond);
	}
}

double ULRGameSubsystem::GetInjectorTarget() const
{
	return Simulation ? Simulation->GetInjectorTarget() : 0.0;
}

void ULRGameSubsystem::SetInjectorAuto(ELRInjectorAuto Mode)
{
	if (Simulation)
	{
		Simulation->SetInjectorAuto(Mode);
	}
}

ELRInjectorAuto ULRGameSubsystem::GetInjectorAuto() const
{
	return Simulation ? Simulation->GetInjectorAuto() : ELRInjectorAuto::Off;
}

FLRActionResult ULRGameSubsystem::SetVenting(bool bVent)
{
	return Simulation ? Simulation->SetVenting(bVent) : FLRActionResult();
}

bool ULRGameSubsystem::IsVenting() const
{
	return Simulation && Simulation->IsVenting();
}

FLRActionResult ULRGameSubsystem::Ignite()
{
	return Simulation ? Simulation->Ignite() : FLRActionResult();
}

float ULRGameSubsystem::GetPlasmaOpacity() const
{
	return Simulation ? Simulation->GetPlasmaOpacity() : 0.f;
}

// ---- Selection ------------------------------------------------------------------------

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

void ULRGameSubsystem::HandleSimMatterChanged()
{
	OnMatterChanged.Broadcast();
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
