#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Simulation/LRSimTypes.h"
#include "LRHud.generated.h"

class SLRGameHud;

/**
 * Owns the Slate HUD widget: adds it to the viewport on BeginPlay, removes it on EndPlay,
 * and feeds it action results for the message log.
 *
 * The GameMode's HUDClass makes the engine spawn one of these per local player.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRHud : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleActionCompleted(const FLRActionResult& Result);

	UFUNCTION()
	void HandleWorldChanged();

private:
	TSharedPtr<SLRGameHud> HudWidget;
};
