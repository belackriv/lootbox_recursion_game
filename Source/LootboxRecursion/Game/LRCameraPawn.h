#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LRCameraPawn.generated.h"

class ALRWorldLineActor;
class UCameraComponent;
class USpringArmComponent;

/**
 * A pawn that is just a camera on a boom. Every frame it glides toward the world cell the
 * subsystem says to focus on. (The player never "walks" in this game - yet.)
 */
UCLASS()
class LOOTBOXRECURSION_API ALRCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ALRCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** How quickly the camera catches up with the focus coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FollowSpeed = 6.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

private:
	TWeakObjectPtr<ALRWorldLineActor> WorldLine;
	bool bSnapped = false;
};
