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
 * feeds it action results for the message log, and passes it the keys the player controller
 * routes to the UI (the command card's slots, the menu key).
 *
 * The GameMode's HUDClass makes the engine spawn one of these per local player.
 */
UCLASS(Config = Game)
class LOOTBOXRECURSION_API ALRHud : public AHUD
{
	GENERATED_BODY()

public:
	/** The Esc menu is open (the game is paused). */
	bool IsMenuOpen() const;
	/** The menu key: back out of whatever is open (help, the Build card), else open or close the menu. */
	void HandleMenuKey();
	/** A command card slot's key (0-based). */
	void ActivateCardSlot(int32 Slot);

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

private:
	TSharedPtr<SLRGameHud> HudWidget;

	/** The loaded static hook, kept alive for the widget (nullptr: plain noise). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StaticHook;
};
