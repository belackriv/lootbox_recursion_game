#include "Game/LRUserSettings.h"

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
