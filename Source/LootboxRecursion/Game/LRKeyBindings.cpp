#include "Game/LRKeyBindings.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "LootboxRecursion.h"
#include "PlayerMappableKeySettings.h"
#include "UObject/UnrealType.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace LRKeyBindings
{
	static EPlayerMappableKeySlot KeySlotFor(int32 Index)
	{
		return Index == 0 ? EPlayerMappableKeySlot::First : EPlayerMappableKeySlot::Second;
	}

	/** Set one slot's key (Invalid clears it) without the one-key-one-command rule. */
	static void MapKey(UEnhancedInputUserSettings& Settings, FName Command, int32 Index, const FKey& Key, FGameplayTagContainer& Failure)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Command;
		Args.Slot = KeySlotFor(Index);
		Args.NewKey = Key;
		Settings.MapPlayerKey(Args, Failure);
	}

	void MakePlayerMappable(UInputAction* Action, const LRInput::FCommand& Command)
	{
		if (!Action)
		{
			return;
		}
		UPlayerMappableKeySettings* KeySettings = NewObject<UPlayerMappableKeySettings>(Action);
		KeySettings->Name = Command.Name;
		KeySettings->DisplayName = FText::FromString(Command.Label);
		KeySettings->DisplayCategory = FText::FromString(Command.Group);
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(UInputAction::StaticClass(), TEXT("PlayerMappableKeySettings")))
		{
			Property->SetObjectPropertyValue_InContainer(Action, KeySettings);
		}
		else
		{
			UE_LOG(LogLootbox, Warning, TEXT("Key rebinding: UInputAction has no PlayerMappableKeySettings property; %s can't be rebound"), *Command.Name.ToString());
		}
	}

	UEnhancedInputUserSettings* GetUserSettings(const UObject* WorldContext)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		const ULocalPlayer* Player = World ? World->GetFirstLocalPlayerFromController() : nullptr;
		const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		return Input ? Input->GetUserSettings() : nullptr;
	}

	FKey GetKey(const UObject* WorldContext, FName Command, int32 Index)
	{
		const UEnhancedInputUserSettings* Settings = GetUserSettings(WorldContext);
		const UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
		if (Profile)
		{
			FMapPlayerKeyArgs Args;
			Args.MappingName = Command;
			Args.Slot = KeySlotFor(Index);
			if (const FPlayerKeyMapping* Mapping = Profile->FindKeyMapping(Args))
			{
				return Mapping->GetCurrentKey();
			}
			// No entry for it (the mapping context isn't registered yet): the defaults apply.
		}
		const LRInput::FCommand* Def = LRInput::FindCommand(Command);
		return Def ? (Index == 0 ? Def->DefaultPrimary : Def->DefaultSecondary) : EKeys::Invalid;
	}

	bool SetKey(const UObject* WorldContext, FName Command, int32 Index, const FKey& Key)
	{
		UEnhancedInputUserSettings* Settings = GetUserSettings(WorldContext);
		if (!Settings)
		{
			return false;
		}
		FGameplayTagContainer Failure;
		if (Key.IsValid())
		{
			// A key does one thing: take it off whatever else has it.
			for (const LRInput::FCommand& Other : LRInput::GetCommands())
			{
				for (int32 OtherIndex = 0; OtherIndex < 2; ++OtherIndex)
				{
					if ((Other.Name != Command || OtherIndex != Index) && GetKey(WorldContext, Other.Name, OtherIndex) == Key)
					{
						MapKey(*Settings, Other.Name, OtherIndex, EKeys::Invalid, Failure);
					}
				}
			}
		}
		MapKey(*Settings, Command, Index, Key.IsValid() ? Key : EKeys::Invalid, Failure);
		Settings->ApplySettings();
		Settings->SaveSettings();
		return Failure.IsEmpty();
	}

	void ResetToDefaults(const UObject* WorldContext)
	{
		UEnhancedInputUserSettings* Settings = GetUserSettings(WorldContext);
		if (!Settings)
		{
			return;
		}
		// The defaults never share a key, so they can go straight back in.
		FGameplayTagContainer Failure;
		for (const LRInput::FCommand& Command : LRInput::GetCommands())
		{
			MapKey(*Settings, Command.Name, 0, Command.DefaultPrimary, Failure);
			MapKey(*Settings, Command.Name, 1, Command.DefaultSecondary, Failure);
		}
		Settings->ApplySettings();
		Settings->SaveSettings();
	}
}
