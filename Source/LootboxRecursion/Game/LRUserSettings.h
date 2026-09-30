#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LRUserSettings.generated.h"

/**
 * The player's settings (Esc menu > Settings), kept apart from the game saves in their own
 * slot (Saved/SaveGames/Settings.sav). Two things live elsewhere, in the engine's own stores:
 * key bindings (Enhanced Input's user settings, see LRKeyBindings) and graphics quality
 * (UGameUserSettings).
 */
UCLASS()
class LOOTBOXRECURSION_API ULRUserSettings : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;

	/** The saved settings, or fresh defaults. */
	static ULRUserSettings* LoadOrCreate();
	bool SaveToDisk();

	/** Free look speed, a multiple of the camera's default. */
	UPROPERTY()
	float LookSensitivity = 1.f;

	/** Free look: moving the mouse up tilts the view down. */
	UPROPERTY()
	bool bInvertLook = false;

	/** Pan speed, a multiple of the camera's default. */
	UPROPERTY()
	float PanSpeed = 1.f;

	/** The HUD's scale. */
	UPROPERTY()
	float UIScale = 1.f;
};
