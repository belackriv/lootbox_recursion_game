#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LRCameraPawn.generated.h"

class ALRWorldLineActor;
class UCameraComponent;
class USpringArmComponent;

/**
 * The player is the creator of this little universe, not a character in it: this pawn is
 * just a "god camera" on a boom. It pans along the world line, zooms and orbits, and keeps
 * the subsystem's focus coordinate (used by the HUD's Deployed list) in sync.
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

	/** -1..1 per frame while held: pan toward screen-left / screen-right. */
	void AddPanInput(float Direction) { PendingPan += Direction; }
	/** Mouse wheel notches: positive zooms in. */
	void AddZoomInput(float Notches);
	/** -1..1 per frame while held: orbit around the focus point. */
	void AddOrbitInput(float Direction) { PendingOrbit += Direction; }
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

	/** Downward tilt of the camera boom, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Pitch = -35.f;

	/** How quickly the camera catches up with its targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FollowSpeed = 8.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

private:
	TWeakObjectPtr<ALRWorldLineActor> WorldLine;
	bool bInitialized = false;

	float CurrentCoordinate = 0.f;
	float TargetCoordinate = 0.f;
	int32 LastWrittenFocus = 0;

	float OrbitYaw = 0.f;
	float TargetOrbitYaw = 0.f;
	float TargetArmLength = 2200.f;

	float PendingPan = 0.f;
	float PendingOrbit = 0.f;
};
