#pragma once

#include "CoreMinimal.h"
#include "Game/LRInputCommands.h"
#include "InputCoreTypes.h"

class UEnhancedInputUserSettings;
class UInputAction;

/**
 * The player's key bindings, through Enhanced Input's user settings (UEnhancedInputUserSettings,
 * enabled in DefaultInput.ini): the engine stores each player's keys in its own save and
 * applies them to the mappings whenever they change.
 *
 * Each rebindable command's action is "player mappable" under the command's name
 * (LRInput::GetCommands()), with its default keys in the First and Second slots. On top of
 * the engine, one rule: a key does one thing, so binding it takes it off anything else.
 *
 * If the user settings aren't available (not enabled, or no local player yet), GetKey reports
 * the defaults and SetKey does nothing.
 */
namespace LRKeyBindings
{
	/**
	 * Make a code-built action player mappable as Command. (An asset would set this in the
	 * editor; UInputAction keeps the property protected, so it's set through reflection.)
	 */
	void MakePlayerMappable(UInputAction* Action, const LRInput::FCommand& Command);

	/** The first local player's input user settings, or null. */
	UEnhancedInputUserSettings* GetUserSettings(const UObject* WorldContext);

	/** Command's key: Index 0 = primary (the First slot), 1 = secondary. EKeys::Invalid if none. */
	FKey GetKey(const UObject* WorldContext, FName Command, int32 Index);

	/**
	 * Bind Key as Command's primary (0) or secondary (1) key, taking it off any other command
	 * first; EKeys::Invalid clears it. Applies and saves. False if the engine refused.
	 */
	bool SetKey(const UObject* WorldContext, FName Command, int32 Index, const FKey& Key);

	/** Every command back to its default keys. Applies and saves. */
	void ResetToDefaults(const UObject* WorldContext);
}
