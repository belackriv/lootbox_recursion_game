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

/** One save in the Load list. */
USTRUCT(BlueprintType)
struct FLRSaveSlotInfo
{
	GENERATED_BODY()

	/** The file's slot name. */
	UPROPERTY(BlueprintReadOnly, Category = "Quantum Recursion|Saves")
	FString SlotName;

	/** What the player called it ("Autosave" for the autosave). */
	UPROPERTY(BlueprintReadOnly, Category = "Quantum Recursion|Saves")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Quantum Recursion|Saves")
	FDateTime SavedAt;

	/** A line about the game in it: epoch, host mass, play time. */
	UPROPERTY(BlueprintReadOnly, Category = "Quantum Recursion|Saves")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "Quantum Recursion|Saves")
	bool bAutosave = false;
};

/**
 * The list of saves (Saved/SaveGames/SaveIndex.sav). Save files can't be listed portably, so
 * the game keeps its own index of them.
 */
UCLASS()
class LOOTBOXRECURSION_API ULRSaveIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FLRSaveSlotInfo> Slots;

	/** Numbers the next manual save's file (Save_1, Save_2...). */
	UPROPERTY()
	int32 NextSlotNumber = 1;
};
