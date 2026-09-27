#include "Game/LRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRWorldGridActor.h"
#include "GameFramework/SpringArmComponent.h"

ALRCameraPawn::ALRCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// The pawn's facing is driven entirely by Tick, never by the controller or the level's
	// PlayerStart, so the default view is always the same.
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

	if (!WorldGrid.IsValid())
	{
		TActorIterator<ALRWorldGridActor> It(GetWorld());
		WorldGrid = It ? *It : nullptr;
	}
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	if (!WorldGrid.IsValid() || !Subsystem)
	{
		return;
	}
	const float CellSize = WorldGrid->CellSize;

	if (!bInitialized)
	{
		CurrentFocus = TargetFocus = WorldGrid->CellToLocal(Subsystem->GetFocusRequestCell());
		LastFocusSerial = Subsystem->GetFocusRequestSerial();
		SpringArm->TargetArmLength = TargetArmLength;
		bInitialized = true;
	}

	// Someone asked to look at a specific cell (HUD list, Home): fly there.
	if (Subsystem->GetFocusRequestSerial() != LastFocusSerial)
	{
		LastFocusSerial = Subsystem->GetFocusRequestSerial();
		TargetFocus = WorldGrid->CellToLocal(Subsystem->GetFocusRequestCell());
	}
	// Always look at the current build layer.
	TargetFocus.Z = Subsystem->GetBuildLayer() * CellSize;

	// Orbit
	TargetOrbitYaw += PendingOrbit * OrbitSpeed * DeltaSeconds;
	PendingOrbit = 0.f;
	OrbitYaw = FMath::FInterpTo(OrbitYaw, TargetOrbitYaw, DeltaSeconds, FollowSpeed);

	// Pan relative to where the camera is facing (grid-local space). Faster when zoomed out.
	const float YawRadians = FMath::DegreesToRadians(OrbitYaw);
	const FVector Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.f);
	const FVector Right(-FMath::Sin(YawRadians), FMath::Cos(YawRadians), 0.f);
	const FVector2D Pan = PendingPan.GetClampedToMaxSize(1.f);
	PendingPan = FVector2D::ZeroVector;
	const float ZoomFactor = SpringArm->TargetArmLength / FMath::Max(DefaultArmLength, 1.f);
	TargetFocus += (Right * Pan.X + Forward * Pan.Y) * PanSpeed * CellSize * ZoomFactor * DeltaSeconds;

	CurrentFocus = FMath::VInterpTo(CurrentFocus, TargetFocus, DeltaSeconds, FollowSpeed);

	// Zoom
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, FollowSpeed);

	SetActorLocation(WorldGrid->GetActorTransform().TransformPosition(CurrentFocus));
	SetActorRotation(FRotator(0.f, WorldGrid->GetActorRotation().Yaw + OrbitYaw, 0.f));
}
