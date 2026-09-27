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
	bEnableClickEvents = true;
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
	UInputAction* OrbitLeftAction = MakeAction(TEXT("IA_OrbitLeft"));
	UInputAction* OrbitRightAction = MakeAction(TEXT("IA_OrbitRight"));
	UInputAction* ZoomAction = MakeAction(TEXT("IA_Zoom"), /*bAxis*/ true);
	UInputAction* HomeAction = MakeAction(TEXT("IA_Home"));
	UInputAction* ResetViewAction = MakeAction(TEXT("IA_ResetView"));

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MappingContext->MapKey(PanLeftAction, EKeys::A);
	MappingContext->MapKey(PanLeftAction, EKeys::Left);
	MappingContext->MapKey(PanRightAction, EKeys::D);
	MappingContext->MapKey(PanRightAction, EKeys::Right);
	MappingContext->MapKey(OrbitLeftAction, EKeys::Q);
	MappingContext->MapKey(OrbitRightAction, EKeys::E);
	MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MappingContext->MapKey(HomeAction, EKeys::H);
	MappingContext->MapKey(HomeAction, EKeys::Home);
	MappingContext->MapKey(ResetViewAction, EKeys::R);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Triggered fires every frame while a key is held; Started fires once per press.
		Input->BindAction(PanLeftAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanLeft);
		Input->BindAction(PanRightAction, ETriggerEvent::Triggered, this, &ALRPlayerController::PanRight);
		Input->BindAction(OrbitLeftAction, ETriggerEvent::Triggered, this, &ALRPlayerController::OrbitLeft);
		Input->BindAction(OrbitRightAction, ETriggerEvent::Triggered, this, &ALRPlayerController::OrbitRight);
		Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Zoom);
		Input->BindAction(HomeAction, ETriggerEvent::Started, this, &ALRPlayerController::Home);
		Input->BindAction(ResetViewAction, ETriggerEvent::Started, this, &ALRPlayerController::ResetView);
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

void ALRPlayerController::PanLeft()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(-1.f); }
}

void ALRPlayerController::PanRight()
{
	if (ALRCameraPawn* CameraPawn = GetCameraPawn()) { CameraPawn->AddPanInput(1.f); }
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

// ---- Console commands -----------------------------------------------------------------

void ALRPlayerController::LRGive(FName Item, int32 Count)
{
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const bool bOk = Subsystem && Subsystem->GiveItem(Item, Count);
	ClientMessage(bOk
		? FString::Printf(TEXT("Gave %d %s"), Count, *Item.ToString())
		: FString::Printf(TEXT("Could not give %d '%s' (unknown item or inventory full) - try LRItems"), Count, *Item.ToString()));
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
