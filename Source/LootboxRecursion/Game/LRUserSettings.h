#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "LRUserSettings.generated.h"

/** A command's keys, as the player bound them (EKeys::Invalid = none). */
USTRUCT()
struct FLRKeyBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName Command;

	UPROPERTY()
	FKey Primary;

	UPROPERTY()
	FKey Secondary;
};

/**
 * The player's settings (Esc menu > Settings), kept apart from the game saves in their own
 * slot (Saved/SaveGames/Settings.sav). Graphics quality isn't here: the engine's
 * UGameUserSettings keeps that.
 *
 * Key bindings are stored only for commands the player changed; everything else uses the
 * defaults in LRInput::GetCommands().
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

	/** A command's keys: the player's binding, or the defaults. */
	void GetKeys(FName Command, FKey& OutPrimary, FKey& OutSecondary) const;
	/** One of them: Index 0 = primary, 1 = secondary. */
	FKey GetKey(FName Command, int32 Index) const;
	/**
	 * Bind Key as Command's primary (Index 0) or secondary (1) key. A key does one thing, so
	 * it's taken off any other command first. EKeys::Invalid clears the binding.
	 */
	void SetKey(FName Command, int32 Index, const FKey& Key);
	/** Back to the default keys. */
	void ResetKeys() { KeyBindings.Reset(); }

	/** Overrides of the default keys. */
	UPROPERTY()
	TArray<FLRKeyBinding> KeyBindings;

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

private:
	FLRKeyBinding& FindOrAddBinding(FName Command);
};
