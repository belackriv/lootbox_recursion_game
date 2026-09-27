#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Simulation/LRSimTypes.h"
#include "LRSaveGame.generated.h"

/**
 * The on-disk save. UE serializes every UPROPERTY automatically; the file lands in
 * Saved/SaveGames/<SlotName>.sav. Rails equivalent: the Postgres database.
 */
UCLASS()
class LOOTBOXRECURSION_API ULRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FLRSaveData Data;
};
