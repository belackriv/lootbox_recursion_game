#include "Game/LRPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/LRCameraPawn.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRHud.h"
#include "Game/LRInputCommands.h"
#include "Game/LRKeyBindings.h"
#include "Game/LRUserSettings.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "LootboxRecursion.h"
#include "UserSettings/EnhancedInputUserSettings.h"

ALRPlayerController::ALRPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ALRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (ULRGameSubsystem* Sub = ULRGameSubsystem::Get(this))
	{
		Sub->OnSettingsChanged.AddDynamic(this, &ALRPlayerController::HandleSettingsChanged);
	}
	ApplyCameraSettings();
	RegisterForRebinding();
}

void ALRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Sub = ULRGameSubsystem::Get(this))
	{
		Sub->OnSettingsChanged.RemoveDynamic(this, &ALRPlayerController::HandleSettingsChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ALRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ApplyCameraSettings();
}

UInputAction* ALRPlayerController::MakeAction(const FString& Name, bool bAxis)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(*Name));
	if (bAxis)
	{
		Action->ValueType = EInputActionValueType::Axis1D;
	}
	InputActions.Add(Action);
	return Action;
}

void ALRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Code-built equivalents of IA_* / IMC_* assets: the fixed mouse controls, then one action
	// per rebindable command (LRInput::GetCommands()).
	ZoomAction = MakeAction(TEXT("IA_Zoom"), /*bAxis*/ true);
	FreeLookAction = MakeAction(TEXT("IA_FreeLook"));
	LookAction = MakeAction(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	SelectAction = MakeAction(TEXT("IA_Select"));

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input)
	{
		Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Zoom);
		Input->BindAction(FreeLookAction, ETriggerEvent::Started, this, &ALRPlayerController::FreeLookStart);
		Input->BindAction(FreeLookAction, ETriggerEvent::Completed, this, &ALRPlayerController::FreeLookEnd);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Look);
		// Clicks on HUD buttons are consumed by Slate and never reach this.
		Input->BindAction(SelectAction, ETriggerEvent::Started, this, &ALRPlayerController::SelectHovered);
	}
	for (const LRInput::FCommand& Command : LRInput::GetCommands())
	{
		UInputAction* Action = MakeAction(FString::Printf(TEXT("IA_%s"), *Command.Name.ToString()));
		LRKeyBindings::MakePlayerMappable(Action, Command);
		CommandActions.Add(Command.Name, Action);
		if (Input)
		{
			// Triggered fires every frame while a key is held; Started fires once per press.
			const FName Name = Command.Name;
			Input->BindActionValueLambda(Action, Command.bHold ? ETriggerEvent::Triggered : ETriggerEvent::Started,
				[this, Name](const FInputActionValue&) { RunCommand(Name); });
		}
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MapDefaultKeys();

	// SetupInputComponent runs once this controller has its LocalPlayer, so the Enhanced Input
	// subsystem is available here.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(MappingContext, 0);
		}
	}
	RegisterForRebinding();
}

void ALRPlayerController::RegisterForRebinding()
{
	if (bRegisteredForRebinding || !MappingContext)
	{
		return;
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputUserSettings* KeySettings = InputSubsystem ? InputSubsystem->GetUserSettings() : nullptr;
	if (!KeySettings)
	{
		return; // not created yet: try again later
	}
	bRegisteredForRebinding = true;
	KeySettings->RegisterInputMappingContext(MappingContext);
	InputSubsystem->RequestRebuildControlMappings(); // apply any keys the player saved
	UE_LOG(LogLootbox, Log, TEXT("Key rebinding: registered %s with Enhanced Input's user settings"), *MappingContext->GetName());
}

void ALRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	RegisterForRebinding();
}

void ALRPlayerController::MapDefaultKeys()
{
	MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MappingContext->MapKey(FreeLookAction, EKeys::RightMouseButton);
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(SelectAction, EKeys::LeftMouseButton);

	// Primary first, then secondary: the order sets their slots (First, Second) in the
	// player's key settings.
	for (const LRInput::FCommand& Command : LRInput::GetCommands())
	{
		const TObjectPtr<UInputAction>* Action = CommandActions.Find(Command.Name);
		if (!Action)
		{
			continue;
		}
		for (const FKey& Key : { Command.DefaultPrimary, Command.DefaultSecondary })
		{
			if (Key.IsValid())
			{
				MappingContext->MapKey(*Action, Key);
			}
		}
	}
}

void ALRPlayerController::ApplyCameraSettings()
{
	const ULRGameSubsystem* Sub = ULRGameSubsystem::Get(this);
	const ULRUserSettings* Settings = Sub ? Sub->GetUserSettings() : nullptr;
	ALRCameraPawn* CameraPawn = GetCameraPawn();
	if (!Settings || !CameraPawn)
	{
		return;
	}
	CameraPawn->PanSpeedScale = Settings->PanSpeed;
	CameraPawn->LookSensitivityScale = Settings->LookSensitivity;
	CameraPawn->bInvertLook = Settings->bInvertLook;
}

void ALRPlayerController::HandleSettingsChanged()
{
	ApplyCameraSettings(); // key changes are applied by Enhanced Input itself
}

ALRCameraPawn* ALRPlayerController::GetCameraPawn() const
{
	return Cast<ALRCameraPawn>(GetPawn());
}

ALRHud* ALRPlayerController::GetLRHud() const
{
	return Cast<ALRHud>(GetHUD());
}

bool ALRPlayerController::IsMenuOpen() const
{
	const ALRHud* Hud = GetLRHud();
	return Hud && Hud->IsMenuOpen();
}

void ALRPlayerController::RunCommand(FName Command)
{
	namespace Names = LRInput::Names;
	ALRHud* Hud = GetLRHud();
	if (Command == Names::Menu)
	{
		if (Hud)
		{
			Hud->HandleMenuKey();
		}
		return;
	}
	if (IsMenuOpen())
	{
		return;
	}

	// While the outside panel is down, the pan keys turn the feed dial instead: left/right in
	// fine steps, up/down in coarse ones.
	const ULRGameSubsystem* Sub = ULRGameSubsystem::Get(this);
	const bool bPan = Command == Names::PanForward || Command == Names::PanBack || Command == Names::PanLeft || Command == Names::PanRight;
	if (bPan && Hud && Sub && Sub->IsOutsideViewOpen())
	{
		const float Direction = (Command == Names::PanForward || Command == Names::PanRight) ? 1.f : -1.f;
		const bool bCoarse = Command == Names::PanForward || Command == Names::PanBack;
		Hud->NudgeDial(Direction, bCoarse, GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
		return;
	}

	if (Command == Names::PanForward)         { PanForward(); }
	else if (Command == Names::PanBack)       { PanBack(); }
	else if (Command == Names::PanLeft)       { PanLeft(); }
	else if (Command == Names::PanRight)      { PanRight(); }
	else if (Command == Names::OrbitLeft)     { OrbitLeft(); }
	else if (Command == Names::OrbitRight)    { OrbitRight(); }
	else if (Command == Names::ResetView)     { ResetView(); }
	else if (Command == Names::LayerUp)       { LayerUp(); }
	else if (Command == Names::LayerDown)     { LayerDown(); }
	else if (Command == Names::Home)          { Home(); }
	else if (Command == Names::ToggleOutside) { ToggleOutside(); }
	else if (Hud)
	{
		for (int32 Slot = 0; Slot < LRInput::CardSlotCount; ++Slot)
		{
			if (Command == LRInput::SlotCommand(Slot))
			{
				Hud->ActivateCardSlot(Slot);
				return;
			}
		}
	}
}

void ALRPlayerController::ToggleOutside()
{
	if (ULRGameSubsystem* Sub = ULRGameSubsystem::Get(this))
	{
		Sub->ToggleOutsideView();
	}
}

void ALRPlayerController::PanLeft()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(FVector2D(-1.f, 0.f)); }
}

void ALRPlayerController::PanRight()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(FVector2D(1.f, 0.f)); }
}

void ALRPlayerController::PanForward()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(FVector2D(0.f, 1.f)); }
}

void ALRPlayerController::PanBack()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(FVector2D(0.f, -1.f)); }
}

void ALRPlayerController::LayerUp()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this)) { Subsystem->ChangeBuildLayer(1); }
}

void ALRPlayerController::LayerDown()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this)) { Subsystem->ChangeBuildLayer(-1); }
}

void ALRPlayerController::OrbitLeft()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddOrbitInput(1.f); }
}

void ALRPlayerController::OrbitRight()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddOrbitInput(-1.f); }
}

void ALRPlayerController::Zoom(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddZoomInput(Value.Get<float>()); }
}

void ALRPlayerController::Home()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->FocusHome();
	}
}

void ALRPlayerController::ResetView()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->ResetView(); }
}

void ALRPlayerController::FreeLookStart()
{
	if (IsMenuOpen())
	{
		return;
	}
	// Hide the cursor while dragging and put it back where it was afterwards.
	bFreeLook = true;
	float CursorX = 0.f;
	float CursorY = 0.f;
	if (GetMousePosition(CursorX, CursorY))
	{
		FreeLookCursorPosition = FVector2D(CursorX, CursorY);
	}
	bShowMouseCursor = false;
}

void ALRPlayerController::FreeLookEnd()
{
	if (!bFreeLook)
	{
		return;
	}
	bFreeLook = false;
	bShowMouseCursor = true;
	SetMouseLocation(FMath::RoundToInt(FreeLookCursorPosition.X), FMath::RoundToInt(FreeLookCursorPosition.Y));
}

void ALRPlayerController::Look(const FInputActionValue& Value)
{
	if (!bFreeLook)
	{
		return;
	}
	if (ALRCameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddLookInput(Value.Get<FVector2D>());
	}
}

void ALRPlayerController::SelectHovered()
{
	if (IsMenuOpen())
	{
		return;
	}
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	FIntVector Hovered;
	if (Subsystem && Subsystem->GetHoveredCell(Hovered))
	{
		Subsystem->SelectCell(Hovered);
	}
}

// ---- Console commands -----------------------------------------------------------------

void ALRPlayerController::LRGive(FName Item, int32 Count)
{
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const bool bOk = Subsystem && Subsystem->GiveMatter(Item, Count);
	ClientMessage(bOk
		? FString::Printf(TEXT("Gave %d %s to the selected cell (or the origin)"), Count, *Item.ToString())
		: FString::Printf(TEXT("Could not give %d '%s' (only materials can be given) - try LRItems"), Count, *Item.ToString()));
}

void ALRPlayerController::LRTimeScale(float Scale)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->SetTimeScale(Scale);
		ClientMessage(FString::Printf(TEXT("Simulation time scale: %.2fx"), Subsystem->GetTimeScale()));
	}
}

void ALRPlayerController::LRReset()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->ResetGame();
		ClientMessage(TEXT("Game reset"));
	}
}

void ALRPlayerController::LRSave()
{
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	ClientMessage(Subsystem && Subsystem->SaveNow() ? TEXT("Saved") : TEXT("Save failed"));
}

void ALRPlayerController::LRItems()
{
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const FLRSimulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
	if (!Simulation)
	{
		return;
	}
	for (const TPair<FName, FLRItemDef>& Pair : Simulation->GetData().Items)
	{
		ClientMessage(FString::Printf(TEXT("%s  (%s)"), *Pair.Key.ToString(), *Pair.Value.DisplayName));
	}
}
