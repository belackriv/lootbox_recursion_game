#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LRCameraPawn.generated.h"

class ALRWorldGridActor;
class UCameraComponent;
class USpringArmComponent;

/**
 * There is no character: you operate the pocket universe from outside the event horizon,
 * so the player is a free "god camera" on a boom. It pans across the build layer, zooms,
 * orbits, follows layer changes, and flies to cells the subsystem asks it to focus on.
 *
 * Input arrives from ALRPlayerController as Add*Input calls and is applied in Tick,
 * the same pattern as APawn::AddMovementInput.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ALRCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** Per frame while held: X = right(+)/left(-), Y = forward(+)/back(-), relative to the view. */
	void AddPanInput(const FVector2D& Direction) { PendingPan += Direction; }
	/** Mouse wheel notches: positive zooms in. */
	void AddZoomInput(float Notches);
	/** -1..1 per frame while held: orbit around the focus point. */
	void AddOrbitInput(float Direction) { PendingOrbit += Direction; }
	/** Mouse delta while free look (right mouse) is held: X orbits, Y tilts. */
	void AddLookInput(const FVector2D& MouseDelta) { PendingLook += MouseDelta; }
	/** Back to the default angle and zoom. */
	void ResetView();

	/** Pan speed in cells per second at the default zoom (scales with zoom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PanSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float OrbitSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float DefaultArmLength = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinArmLength = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxArmLength = 9000.f;

	/** Default downward tilt of the camera boom, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Pitch = -50.f;

	/**
	 * Tilt limits for free look, in degrees. Negative looks down on the focus point from above;
	 * positive swings the camera below the build layer to look up through it at the sky.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinPitch = -88.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxPitch = 80.f;

	/** Degrees per unit of mouse movement while free looking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float LookSensitivity = 0.25f;

	/** How quickly the camera catches up with its targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FollowSpeed = 8.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

private:
	TWeakObjectPtr<ALRWorldGridActor> WorldGrid;
	bool bInitialized = false;

	/** Focus point in grid-local space (cm). The pawn sits here; the boom looks at it. */
	FVector CurrentFocus = FVector::ZeroVector;
	FVector TargetFocus = FVector::ZeroVector;
	int32 LastFocusSerial = 0;

	float OrbitYaw = 0.f;
	float TargetOrbitYaw = 0.f;
	float CurrentPitch = -50.f;
	float TargetPitch = -50.f;
	float TargetArmLength = 2200.f;

	FVector2D PendingPan = FVector2D::ZeroVector;
	float PendingOrbit = 0.f;
	FVector2D PendingLook = FVector2D::ZeroVector;
};
