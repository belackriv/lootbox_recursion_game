#include "Game/LRUserSettings.h"

#include "Game/LRInputCommands.h"
#include "Kismet/GameplayStatics.h"

const FString ULRUserSettings::SlotName = TEXT("Settings");

ULRUserSettings* ULRUserSettings::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (ULRUserSettings* Loaded = Cast<ULRUserSettings>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			return Loaded;
		}
	}
	return Cast<ULRUserSettings>(UGameplayStatics::CreateSaveGameObject(ULRUserSettings::StaticClass()));
}

bool ULRUserSettings::SaveToDisk()
{
	return UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}

void ULRUserSettings::GetKeys(FName Command, FKey& OutPrimary, FKey& OutSecondary) const
{
	if (const FLRKeyBinding* Binding = KeyBindings.FindByPredicate([Command](const FLRKeyBinding& Each) { return Each.Command == Command; }))
	{
		OutPrimary = Binding->Primary;
		OutSecondary = Binding->Secondary;
		return;
	}
	const LRInput::FCommand* Def = LRInput::FindCommand(Command);
	OutPrimary = Def ? Def->DefaultPrimary : EKeys::Invalid;
	OutSecondary = Def ? Def->DefaultSecondary : EKeys::Invalid;
}

FKey ULRUserSettings::GetKey(FName Command, int32 Index) const
{
	FKey Primary;
	FKey Secondary;
	GetKeys(Command, Primary, Secondary);
	return Index == 0 ? Primary : Secondary;
}

FLRKeyBinding& ULRUserSettings::FindOrAddBinding(FName Command)
{
	if (FLRKeyBinding* Binding = KeyBindings.FindByPredicate([Command](const FLRKeyBinding& Each) { return Each.Command == Command; }))
	{
		return *Binding;
	}
	FLRKeyBinding Added;
	Added.Command = Command;
	GetKeys(Command, Added.Primary, Added.Secondary);
	return KeyBindings.Add_GetRef(Added);
}

void ULRUserSettings::SetKey(FName Command, int32 Index, const FKey& Key)
{
	if (Key.IsValid())
	{
		// Take the key off whatever else had it.
		for (const LRInput::FCommand& Other : LRInput::GetCommands())
		{
			for (int32 OtherIndex = 0; OtherIndex < 2; ++OtherIndex)
			{
				if ((Other.Name != Command || OtherIndex != Index) && GetKey(Other.Name, OtherIndex) == Key)
				{
					FLRKeyBinding& Taken = FindOrAddBinding(Other.Name);
					(OtherIndex == 0 ? Taken.Primary : Taken.Secondary) = EKeys::Invalid;
				}
			}
		}
	}
	FLRKeyBinding& Binding = FindOrAddBinding(Command);
	(Index == 0 ? Binding.Primary : Binding.Secondary) = Key.IsValid() ? Key : EKeys::Invalid;
}
