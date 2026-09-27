#include "Game/LRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRWorldLineActor.h"
#include "GameFramework/SpringArmComponent.h"

ALRCameraPawn::ALRCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Don't let the controller's rotation spin the pawn; the boom angle is fixed.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 2200.f;
	SpringArm->SetRelativeRotation(FRotator(-30.f, 0.f, 0.f)); // look down the +X axis at the line
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(60.f);
}

void ALRCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!WorldLine.IsValid())
	{
		TActorIterator<ALRWorldLineActor> It(GetWorld());
		WorldLine = It ? *It : nullptr;
	}
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	if (!WorldLine.IsValid() || !Subsystem)
	{
		return;
	}

	const FVector Target = WorldLine->GetCellWorldLocation(Subsystem->GetFocusCoordinate());
	if (!bSnapped)
	{
		SetActorLocation(Target);
		bSnapped = true;
	}
	else
	{
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, FollowSpeed));
	}
}
