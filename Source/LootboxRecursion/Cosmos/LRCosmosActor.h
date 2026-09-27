#pragma once

#include "CoreMinimal.h"
#include "Cosmos/LRBlackHoleRenderer.h"
#include "GameFramework/Actor.h"
#include "LRCosmosActor.generated.h"

class UInstancedStaticMeshComponent;
class ULevel;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UWorld;

/**
 * The view out of the pocket universe: a black void, a starfield with a faint galactic band,
 * and the animated black hole (see FLRBlackHoleRenderer). Everything is at "infinity": the
 * actor follows the camera, so you can never get closer to it.
 *
 * Uses only engine content. Stars and the black hole use the engine's unlit 3D-widget
 * materials, so they glow without lights and bloom when bright. Settings are Config, so they
 * can be tweaked in Config/DefaultGame.ini under [/Script/LootboxRecursion.LRCosmosActor].
 */
UCLASS(Config = Game)
class LOOTBOXRECURSION_API ALRCosmosActor : public AActor
{
	GENERATED_BODY()

public:
	ALRCosmosActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Radius of the sky, in cm. Everything else is placed inside it. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float SkyRadius = 900000.f;

	/**
	 * Direction of the black hole relative to the grid (+X is where the camera looks by
	 * default). Slightly below the horizon, so it looms behind the grid at the default tilt.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	FVector BlackHoleDirection = FVector(1.f, 0.3f, -0.85f);

	/** Angular width of the black hole image, in degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float BlackHoleAngularSize = 45.f;

	/** Rotates the black hole image around the view direction (degrees). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float BlackHoleRoll = 0.f;

	/** HDR multiplier for the disk and stars; above 1 makes them bloom. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float Brightness = 2.5f;

	/** Fixed exposure compensation in EV (auto exposure is off while the cosmos is up). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float ExposureBias = 0.f;

	/** How often the disk animation is re-rendered. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float FramesPerSecond = 30.f;

	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	int32 StarCount = 3000;

	/**
	 * While playing, hide everything visible the level brings: sky atmosphere, clouds, fog,
	 * floors, landscapes and props (template levels have all of them). The game happens in the
	 * void; lights keep working and the level asset itself is untouched. Turn this off to build
	 * scenery in a level.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	bool bHideLevelEnvironment = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UInstancedStaticMeshComponent* MakeStarLayer(const FLinearColor& Tint);
	void BuildStars();
	void BuildBlackHole();
	void UploadFrame();
	void HideLevelEnvironment();
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void HideLevelContent(ULevel* Level);

	FDelegateHandle LevelAddedHandle;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	/** Engine materials used by 3D UI widgets: unlit, with a "SlateUI" texture and a tint. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitOpaqueMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitTranslucentMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Backdrop;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> StarLayers;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BlackHole;

	UPROPERTY()
	TObjectPtr<UTexture2D> WhiteTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> BlackHoleTexture;

	FLRBlackHoleRenderer Renderer;
	TArray<FColor> FramePixels;
	double AnimationTime = 0.0;
	double SinceLastFrame = 0.0;
};
