#include "Game/LRPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/LRCameraPawn.h"
#include "Game/LRGameSubsystem.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

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
}

UInputAction* ALRPlayerController::MakeAction(const TCHAR* Name, bool bAxis)
{
	UInputAction* Action = NewObject<UInputAction>(this, Name);
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

	// Code-built equivalents of IA_* / IMC_* assets.
	UInputAction* PanLeftAction = MakeAction(TEXT("IA_PanLeft"));
	UInputAction* PanRightAction = MakeAction(TEXT("IA_PanRight"));
	UInputAction* PanForwardAction = MakeAction(TEXT("IA_PanForward"));
	UInputAction* PanBackAction = MakeAction(TEXT("IA_PanBack"));
	UInputAction* LayerUpAction = MakeAction(TEXT("IA_LayerUp"));
	UInputAction* LayerDownAction = MakeAction(TEXT("IA_LayerDown"));
	UInputAction* OrbitLeftAction = MakeAction(TEXT("IA_OrbitLeft"));
	UInputAction* OrbitRightAction = MakeAction(TEXT("IA_OrbitRight"));
	UInputAction* ZoomAction = MakeAction(TEXT("IA_Zoom"), /*bAxis*/ true);
	UInputAction* HomeAction = MakeAction(TEXT("IA_Home"));
	UInputAction* ResetViewAction = MakeAction(TEXT("IA_ResetView"));
	UInputAction* FreeLookAction = MakeAction(TEXT("IA_FreeLook"));
	UInputAction* LookAction = MakeAction(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	UInputAction* SelectAction = MakeAction(TEXT("IA_Select"));
	UInputAction* OutsideAction = MakeAction(TEXT("IA_ToggleOutside"));

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MappingContext->MapKey(PanLeftAction, EKeys::A);
	MappingContext->MapKey(PanLeftAction, EKeys::Left);
	MappingContext->MapKey(PanRightAction, EKeys::D);
	MappingContext->MapKey(PanRightAction, EKeys::Right);
	MappingContext->MapKey(PanForwardAction, EKeys::W);
	MappingContext->MapKey(PanForwardAction, EKeys::Up);
	MappingContext->MapKey(PanBackAction, EKeys::S);
	MappingContext->MapKey(PanBackAction, EKeys::Down);
	MappingContext->MapKey(LayerUpAction, EKeys::PageUp);
	MappingContext->MapKey(LayerUpAction, EKeys::RightBracket);
	MappingContext->MapKey(LayerDownAction, EKeys::PageDown);
	MappingContext->MapKey(LayerDownAction, EKeys::LeftBracket);
	MappingContext->MapKey(OrbitLeftAction, EKeys::Q);
	MappingContext->MapKey(OrbitRightAction, EKeys::E);
	MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MappingContext->MapKey(HomeAction, EKeys::H);
	MappingContext->MapKey(HomeAction, EKeys::Home);
	MappingContext->MapKey(ResetViewAction, EKeys::R);
	MappingContext->MapKey(FreeLookAction, EKeys::RightMouseButton);
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(SelectAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(OutsideAction, EKeys::F);
	MappingContext->MapKey(OutsideAction, EKeys::Tab);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Triggered fires every frame while a key is held; Started fires once per press.
		Input->BindAction(PanLeftAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanLeft);
		Input->BindAction(PanRightAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanRight);
		Input->BindAction(PanForwardAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanForward);
		Input->BindAction(PanBackAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanBack);
		Input->BindAction(LayerUpAction, ETriggerEvent::Started, this, &ALRPlayerController::LayerUp);
		Input->BindAction(LayerDownAction, ETriggerEvent::Started, this, &ALRPlayerController::LayerDown);
		Input->BindAction(OrbitLeftAction, ETriggerEvent::Triggered, this, &ALRPlayerController::OrbitLeft);
		Input->BindAction(OrbitRightAction, ETriggerEvent::Triggered, this, &ALRPlayerController::OrbitRight);
		Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Zoom);
		Input->BindAction(HomeAction, ETriggerEvent::Started, this, &ALRPlayerController::Home);
		Input->BindAction(ResetViewAction, ETriggerEvent::Started, this, &ALRPlayerController::ResetView);
		Input->BindAction(FreeLookAction, ETriggerEvent::Started, this, &ALRPlayerController::FreeLookStart);
		Input->BindAction(FreeLookAction, ETriggerEvent::Completed, this, &ALRPlayerController::FreeLookEnd);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Look);
		// Clicks on HUD buttons are consumed by Slate and never reach this.
		Input->BindAction(SelectAction, ETriggerEvent::Started, this, &ALRPlayerController::SelectHovered);
		Input->BindAction(OutsideAction, ETriggerEvent::Started, this, &ALRPlayerController::ToggleOutside);
	}

	// SetupInputComponent runs once this controller has its LocalPlayer, so the
	// Enhanced Input subsystem is available here.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

ALRCameraPawn* ALRPlayerController::GetCameraPawn() const
{
	return Cast<ALRCameraPawn>(GetPawn());
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
