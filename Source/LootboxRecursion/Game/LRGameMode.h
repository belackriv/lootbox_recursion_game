#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LRGameMode.generated.h"

/**
 * Wires the game together: which pawn, controller and HUD classes to use, and makes sure
 * the world view and some lighting exist even in an empty level.
 *
 * Set as GlobalDefaultGameMode in Config/DefaultEngine.ini, so it applies to every map
 * unless a level's World Settings override it.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALRGameMode();

	virtual void StartPlay() override;

private:
	void EnsureWorldView();
	void EnsureLighting();
};
