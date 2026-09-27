#include "Game/LRPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
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

void ALRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Code-built equivalents of IA_* / IMC_* assets.
	PanLeftAction = NewObject<UInputAction>(this, TEXT("IA_PanLeft"));
	PanRightAction = NewObject<UInputAction>(this, TEXT("IA_PanRight"));
	HomeAction = NewObject<UInputAction>(this, TEXT("IA_Home"));
	ScrollAction = NewObject<UInputAction>(this, TEXT("IA_Scroll"));
	ScrollAction->ValueType = EInputActionValueType::Axis1D;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MappingContext->MapKey(PanLeftAction, EKeys::A);
	MappingContext->MapKey(PanLeftAction, EKeys::Left);
	MappingContext->MapKey(PanRightAction, EKeys::D);
	MappingContext->MapKey(PanRightAction, EKeys::Right);
	MappingContext->MapKey(HomeAction, EKeys::Home);
	MappingContext->MapKey(HomeAction, EKeys::H);
	MappingContext->MapKey(ScrollAction, EKeys::MouseWheelAxis);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(PanLeftAction, ETriggerEvent::Started, this, &ALRPlayerController::PanLeft);
		Input->BindAction(PanRightAction, ETriggerEvent::Started, this, &ALRPlayerController::PanRight);
		Input->BindAction(HomeAction, ETriggerEvent::Started, this, &ALRPlayerController::Home);
		Input->BindAction(ScrollAction, ETriggerEvent::Triggered, this, &ALRPlayerController::Scroll);
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

void ALRPlayerController::PanLeft()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->PanFocus(-1);
	}
}

void ALRPlayerController::PanRight()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->PanFocus(1);
	}
}

void ALRPlayerController::Scroll(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->PanFocus(Axis > 0.f ? -2 : 2);
	}
}

void ALRPlayerController::Home()
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->FocusHome();
	}
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
