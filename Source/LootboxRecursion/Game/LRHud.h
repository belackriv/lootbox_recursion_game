#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Simulation/LRSimTypes.h"
#include "UObject/SoftObjectPath.h"
#include "LRHud.generated.h"

class SLRGameHud;
class UMaterialInterface;

/**
 * Owns the Slate HUD widget: adds it to the viewport on BeginPlay, removes it on EndPlay,
 * and feeds it action results for the message log.
 *
 * The GameMode's HUDClass makes the engine spawn one of these per local player.
 */
UCLASS(Config = Game)
class LOOTBOXRECURSION_API ALRHud : public AHUD
{
	GENERATED_BODY()

public:
	/**
	 * Material hook (see LRMaterialHooks and docs/MATERIALS.md) for the TV static over the 3D
	 * view and the chamber camera while the instruments are down. Used if the asset exists.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Materials")
	FSoftObjectPath StaticMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_Static.M_Static"));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleActionCompleted(const FLRActionResult& Result);

	UFUNCTION()
	void HandleWorldChanged();

private:
	TSharedPtr<SLRGameHud> HudWidget;

	/** The loaded static hook, kept alive for the widget (nullptr: plain noise). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StaticHook;
};
