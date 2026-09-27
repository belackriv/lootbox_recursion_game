#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LRPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Mouse-driven controller: cursor visible, click events on (so world tiles can be clicked),
 * keyboard / mouse wheel pans along the world line, and a few console cheats.
 *
 * Input uses Enhanced Input. Normally Input Actions and Mapping Contexts are assets you
 * create in the editor; here they are built in code so the project runs with zero assets.
 * See docs/UNREAL_PRIMER.md for how to move them to assets.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALRPlayerController();

	// ---- Console commands: press ~ in game and type e.g. "LRGive wood 500" -------------
	UFUNCTION(Exec)
	void LRGive(FName Item, int32 Count);

	UFUNCTION(Exec)
	void LRTimeScale(float Scale);

	UFUNCTION(Exec)
	void LRReset();

	UFUNCTION(Exec)
	void LRSave();

	UFUNCTION(Exec)
	void LRItems();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void PanLeft();
	void PanRight();
	void Scroll(const FInputActionValue& Value);
	void Home();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ScrollAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HomeAction;
};
