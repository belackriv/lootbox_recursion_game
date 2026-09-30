#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The rebindable keyboard commands (Esc menu > Settings > Controls) and their default keys.
 * ALRPlayerController builds its Enhanced Input mappings from these and the player's overrides
 * (ULRUserSettings). The mouse controls (wheel zoom, right-drag free look, left-click select)
 * are fixed.
 *
 * The command card's slots are commands too (Slot1..Slot10, keys 1-0 by default): a slot's key
 * stays the same, and what it does depends on what's selected (docs/DESIGN.md, "The command
 * card").
 */
namespace LRInput
{
	struct FCommand
	{
		FName Name;
		FString Label;
		/** The heading it's listed under in Settings. */
		FString Group;
		FKey DefaultPrimary;
		FKey DefaultSecondary;
		/** Fires every frame while held (camera movement); otherwise once per press. */
		bool bHold = false;
	};

	/** How many slots the command card has. */
	constexpr int32 CardSlotCount = 10;

	namespace Names
	{
		inline const FName PanForward(TEXT("PanForward"));
		inline const FName PanBack(TEXT("PanBack"));
		inline const FName PanLeft(TEXT("PanLeft"));
		inline const FName PanRight(TEXT("PanRight"));
		inline const FName OrbitLeft(TEXT("OrbitLeft"));
		inline const FName OrbitRight(TEXT("OrbitRight"));
		inline const FName ResetView(TEXT("ResetView"));
		inline const FName LayerUp(TEXT("LayerUp"));
		inline const FName LayerDown(TEXT("LayerDown"));
		inline const FName Home(TEXT("Home"));
		inline const FName ToggleOutside(TEXT("ToggleOutside"));
		inline const FName Menu(TEXT("Menu"));
	}

	/** The command for card slot Index (0-based): Slot1..Slot10. */
	inline FName SlotCommand(int32 Index)
	{
		return FName(*FString::Printf(TEXT("Slot%d"), Index + 1));
	}

	/** Every rebindable command, in the order Settings lists them. */
	inline const TArray<FCommand>& GetCommands()
	{
		static const TArray<FCommand> Commands = []()
		{
			const FString Camera(TEXT("Camera"));
			const FString Grid(TEXT("Grid"));
			const FString Card(TEXT("Command card"));
			const FString General(TEXT("General"));
			TArray<FCommand> Out;
			Out.Add({ Names::PanForward, TEXT("Pan forward"), Camera, EKeys::W, EKeys::Up, true });
			Out.Add({ Names::PanBack, TEXT("Pan back"), Camera, EKeys::S, EKeys::Down, true });
			Out.Add({ Names::PanLeft, TEXT("Pan left"), Camera, EKeys::A, EKeys::Left, true });
			Out.Add({ Names::PanRight, TEXT("Pan right"), Camera, EKeys::D, EKeys::Right, true });
			Out.Add({ Names::OrbitLeft, TEXT("Orbit left"), Camera, EKeys::Q, EKeys::Invalid, true });
			Out.Add({ Names::OrbitRight, TEXT("Orbit right"), Camera, EKeys::E, EKeys::Invalid, true });
			Out.Add({ Names::ResetView, TEXT("Reset view"), Camera, EKeys::R, EKeys::Invalid, false });
			Out.Add({ Names::LayerUp, TEXT("Layer up"), Grid, EKeys::PageUp, EKeys::RightBracket, false });
			Out.Add({ Names::LayerDown, TEXT("Layer down"), Grid, EKeys::PageDown, EKeys::LeftBracket, false });
			Out.Add({ Names::Home, TEXT("Fly home"), Grid, EKeys::H, EKeys::Home, false });
			const FKey SlotKeys[CardSlotCount] = {
				EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
				EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero,
			};
			for (int32 Index = 0; Index < CardSlotCount; ++Index)
			{
				Out.Add({ SlotCommand(Index), FString::Printf(TEXT("Slot %d"), Index + 1), Card, SlotKeys[Index], EKeys::Invalid, false });
			}
			Out.Add({ Names::ToggleOutside, TEXT("Outside panel"), General, EKeys::F, EKeys::Tab, false });
			// In the editor's Play-in-Editor, Esc stops play, so F10 opens the menu there.
			Out.Add({ Names::Menu, TEXT("Menu / back"), General, EKeys::Escape, EKeys::F10, false });
			return Out;
		}();
		return Commands;
	}

	inline const FCommand* FindCommand(FName Name)
	{
		return GetCommands().FindByPredicate([Name](const FCommand& Command) { return Command.Name == Name; });
	}
}
