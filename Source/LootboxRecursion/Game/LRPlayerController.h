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
 * The keyboard commands and their default keys are in LRInput::GetCommands(). Each one's
 * action is player mappable, and the mapping context is registered with Enhanced Input's user
 * settings, so the player can rebind them (Esc menu > Settings) and the engine keeps and
 * applies their keys (see LRKeyBindings).
 * By default:
 *   WASD / arrows (hold)      pan across the build layer
 *   Q/E (hold)                orbit
 *   R                         reset camera angle and zoom
 *   PageUp/PageDown or ] [    build layer up / down
 *   H / Home                  fly to the first deployed entity
 *   1-9, 0                    command card slots (the outside panel's buttons while it's down)
 *   F / Tab                   drop the outside panel (the feed dial) down, or raise it; while
 *                             it's down, WASD / arrows turn the dial (A/D fine, W/S coarse)
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
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	class ALRCameraPawn* GetCameraPawn() const;
	class ALRHud* GetLRHud() const;
	/** The Esc menu is open: the game ignores everything but the menu key. */
	bool IsMenuOpen() const;

	/** Map the default keys (and the fixed mouse controls); the player's own keys come from Enhanced Input's user settings. */
	void MapDefaultKeys();
	/**
	 * Register the mapping context with Enhanced Input's user settings, which makes its keys
	 * rebindable and applies the player's saved ones. The settings may not exist yet when input
	 * is set up, so this is retried (BeginPlay, then every tick) until it works.
	 */
	void RegisterForRebinding();
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
	bool bRegisteredForRebinding = false;
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
