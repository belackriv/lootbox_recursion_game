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

	/**
	 * Seconds for the disk's inner edge to go round once; the outer edge is about 8 times slower.
	 * Set it very low (0.5) to check the animation reaches the screen at all.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float DiskSpinSeconds = 3.f;

	/** How often the disk animation is re-rendered. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float FramesPerSecond = 30.f;

	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	int32 StarCount = 3000;

	/**
	 * Glow of the primordial plasma that fills the sky before recombination (HDR: above 1
	 * blooms). Its opacity comes from the current epoch; it clears when the universe turns
	 * transparent.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	FLinearColor PlasmaColor = FLinearColor(1.6f, 0.75f, 0.35f);

	/** Opacity of the plasma at its thickest, so the black hole stays faintly visible through it. */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	float MaxPlasmaOpacity = 0.92f;

	/**
	 * While playing, hide everything visible the level brings: sky atmosphere, clouds, fog,
	 * floors, landscapes and props (template levels have all of them). The game happens in the
	 * void; lights keep working and the level asset itself is untouched. Turn this off to build
	 * scenery in a level.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos")
	bool bHideLevelEnvironment = true;

	/*
	 * Material hooks (see LRMaterialHooks and docs/MATERIALS.md): each is used if the asset
	 * exists, otherwise the built-in flat unlit look stays. All of these are seen from inside
	 * the sky, so make them Two Sided (or use a sphere with inward faces).
	 */

	/** The void behind everything. Opaque, unlit; gets Color (near black). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos|Materials")
	FSoftObjectPath BackdropMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_SkyBackdrop.M_SkyBackdrop"));

	/** The stars (instanced cubes). Unlit, "Used with Instanced Static Meshes"; gets Color (HDR, one per star layer). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos|Materials")
	FSoftObjectPath StarMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_Star.M_Star"));

	/** The black hole plane. Translucent, unlit; gets Texture (the animated frames, alpha = coverage) and Color (Brightness). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos|Materials")
	FSoftObjectPath BlackHoleMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_BlackHole.M_BlackHole"));

	/** The primordial plasma veil. Translucent, unlit, Two Sided; gets Color (PlasmaColor) and Opacity (from the epoch, live). */
	UPROPERTY(Config, EditAnywhere, Category = "Cosmos|Materials")
	FSoftObjectPath PlasmaMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_Plasma.M_Plasma"));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UInstancedStaticMeshComponent* MakeStarLayer(const FLinearColor& Tint);
	void BuildStars();
	void BuildBlackHole();
	void BuildPlasmaVeil();
	void UpdatePlasmaVeil();
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

	/** The hooks that exist (nullptr for the ones that don't); see the *MaterialPath settings. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BackdropHook;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> StarHook;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BlackHoleHook;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlasmaHook;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Backdrop;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> StarLayers;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BlackHole;

	/** A sphere inside the stars that glows with the primordial plasma (see PlasmaColor). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlasmaVeil;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlasmaMaterial;

	/** Opacity last applied to the veil (-1 = never), so the material only updates on change. */
	float ShownPlasmaOpacity = -1.f;

	UPROPERTY()
	TObjectPtr<UTexture2D> WhiteTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> BlackHoleTexture;

	FLRBlackHoleRenderer Renderer;
	TArray<FColor> FramePixels;
	double AnimationTime = 0.0;
	double SinceLastFrame = 0.0;
};
