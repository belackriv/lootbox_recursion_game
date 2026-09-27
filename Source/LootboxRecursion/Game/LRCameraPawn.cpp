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

	// The pawn's facing is driven entirely by Tick (never by the controller or the level's
	// PlayerStart), so the line always reads left-to-right at the default angle.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = DefaultArmLength;
	SpringArm->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f)); // boom points back along -X, camera looks down +X
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(60.f);

	TargetArmLength = DefaultArmLength;
}

void ALRCameraPawn::AddZoomInput(float Notches)
{
	// Multiplicative zoom feels even at every distance.
	TargetArmLength = FMath::Clamp(TargetArmLength * FMath::Pow(0.85f, Notches), MinArmLength, MaxArmLength);
}

void ALRCameraPawn::ResetView()
{
	TargetOrbitYaw = 0.f;
	TargetArmLength = DefaultArmLength;
}

void ALRCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!WorldLine.IsValid())
	{
		TActorIterator<ALRWorldLineActor> It(GetWorld());
		WorldLine = It ? *It : nullptr;
	}
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	if (!WorldLine.IsValid() || !Subsystem)
	{
		return;
	}

	if (!bInitialized)
	{
		CurrentCoordinate = TargetCoordinate = static_cast<float>(Subsystem->GetFocusCoordinate());
		LastWrittenFocus = Subsystem->GetFocusCoordinate();
		SpringArm->TargetArmLength = TargetArmLength;
		bInitialized = true;
	}

	// Someone else moved the focus (HUD paging buttons, Home): jump the target there.
	if (Subsystem->GetFocusCoordinate() != LastWrittenFocus)
	{
		TargetCoordinate = static_cast<float>(Subsystem->GetFocusCoordinate());
	}

	// Orbit
	TargetOrbitYaw += PendingOrbit * OrbitSpeed * DeltaSeconds;
	PendingOrbit = 0.f;
	OrbitYaw = FMath::FInterpTo(OrbitYaw, TargetOrbitYaw, DeltaSeconds, FollowSpeed);

	// Pan: "right" on screen maps to +coordinate when viewing from the default side and to
	// -coordinate when orbited round to the other side. Pan faster when zoomed out.
	const float ScreenSign = FMath::Cos(FMath::DegreesToRadians(OrbitYaw)) >= 0.f ? 1.f : -1.f;
	const float ZoomFactor = SpringArm->TargetArmLength / FMath::Max(DefaultArmLength, 1.f);
	TargetCoordinate += PendingPan * ScreenSign * PanSpeed * ZoomFactor * DeltaSeconds;
	PendingPan = 0.f;
	CurrentCoordinate = FMath::FInterpTo(CurrentCoordinate, TargetCoordinate, DeltaSeconds, FollowSpeed);

	Subsystem->SetFocusCoordinate(FMath::RoundToInt(TargetCoordinate));
	LastWrittenFocus = Subsystem->GetFocusCoordinate();

	// Zoom
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, FollowSpeed);

	SetActorLocation(WorldLine->GetCellWorldLocation(CurrentCoordinate));
	SetActorRotation(FRotator(0.f, WorldLine->GetActorRotation().Yaw + OrbitYaw, 0.f));
}
