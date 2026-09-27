#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LRPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Mouse-driven "god mode" controller: cursor visible, click events on (so world tiles can be
 * clicked), camera controls forwarded to ALRCameraPawn, and a few console cheats.
 *
 *   WASD / arrows (hold)      pan across the build layer
 *   Mouse wheel               zoom
 *   Q/E (hold)                orbit
 *   Right mouse (hold + drag) free look: orbit and tilt
 *   Left mouse                select the hovered grid cell
 *   PageUp/PageDown or ] [    build layer up / down
 *   H / Home                  fly to the first deployed entity
 *   R                         reset camera angle and zoom
 *
 * Input uses Enhanced Input. Normally Input Actions and Mapping Contexts are assets you
 * create in the editor; here they are built in code so the project runs with zero assets.
 * See docs/UNREAL_PRIMER.md for how to move them to assets.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALRPlayerController();

	// ---- Console commands: press ~ in game and type e.g. "LRGive carbon 500" -----------
	UFUNCTION(Exec)
	void LRGive(FName Item, int32 Count);

	UFUNCTION(Exec)
	void LRTimeScale(float Scale);

	UFUNCTION(Exec)
	void LRReset();

	UFUNCTION(Exec)
	void LRSave();

	UFUNCTION(Exec)
	void LRItems();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	class ALRCameraPawn* GetCameraPawn() const;

	void PanLeft();
	void PanRight();
	void PanForward();
	void PanBack();
	void LayerUp();
	void LayerDown();
	void OrbitLeft();
	void OrbitRight();
	void Zoom(const FInputActionValue& Value);
	void Home();
	void ResetView();
	void FreeLookStart();
	void FreeLookEnd();
	void Look(const FInputActionValue& Value);
	void SelectHovered();

	bool bFreeLook = false;
	FVector2D FreeLookCursorPosition = FVector2D::ZeroVector;

	UInputAction* MakeAction(const TCHAR* Name, bool bAxis = false);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Keeps the runtime-created UInputActions alive (UPROPERTY = GC root). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> InputActions;
};
