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
 * The keyboard commands and their default keys are in LRInput::GetCommands(); the player can
 * rebind them (Esc menu > Settings), and the mappings are rebuilt whenever the settings change.
 * By default:
 *   WASD / arrows (hold)      pan across the build layer
 *   Q/E (hold)                orbit
 *   R                         reset camera angle and zoom
 *   PageUp/PageDown or ] [    build layer up / down
 *   H / Home                  fly to the first deployed entity
 *   1-9, 0                    command card slots
 *   F / Tab                   drop the outside panel (the feed dial) down, or raise it
 *   Esc / F10                 back out of the Build card, or the menu (pauses)
 * Fixed:
 *   Mouse wheel               zoom
 *   Right mouse (hold + drag) free look: orbit and tilt
 *   Left mouse                select the hovered grid cell
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

	// ---- Console commands: press ~ in game and type e.g. "LRGive hydrogen 500" ---------
	/** Adds a material to the selected cell (or the origin). */
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

private:
	class ALRCameraPawn* GetCameraPawn() const;
	class ALRHud* GetLRHud() const;
	/** The Esc menu is open: the game ignores everything but the menu key. */
	bool IsMenuOpen() const;

	/** Rebuild the key mappings from the player's bindings (and the fixed mouse controls). */
	void ApplyKeyBindings();
	/** Hand the camera settings to the camera pawn. */
	void ApplyCameraSettings();
	UFUNCTION()
	void HandleSettingsChanged();
	/** A rebindable command's key went down (or is held, for camera movement). */
	void RunCommand(FName Command);

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
	void ToggleOutside();

	bool bFreeLook = false;
	FVector2D FreeLookCursorPosition = FVector2D::ZeroVector;

	UInputAction* MakeAction(const FString& Name, bool bAxis = false);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** The fixed mouse actions, and one action per rebindable command. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FreeLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectAction;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> CommandActions;

	/** Keeps the runtime-created UInputActions alive (UPROPERTY = GC root). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> InputActions;
};
