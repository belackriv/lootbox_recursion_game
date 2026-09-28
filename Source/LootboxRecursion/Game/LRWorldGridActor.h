#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LRWorldGridActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;

/**
 * 3D view of the pocket universe's grid: hexagonal cells in layers, addressed as
 * (Q, R, Layer) (see FLRHexGrid for the geometry). Rails/Vue equivalent: WorldGrid.vue +
 * WorldCellSlot.vue, now hexagonal and layered.
 *
 * - Draws the current build layer's hexagons around the camera focus as thin, glowing beams
 *   of light (unlit instanced cubes, one draw call per brightness step), fading out towards
 *   the edge. The patch is built once and only moved, a whole cell at a time, when the camera
 *   has panned a few cells; it is rebuilt when zooming changes the beam width.
 * - Draws one mesh per entity, on every layer, and a disc of gas in every cell that holds
 *   matter: sized by how much, tinted by the mix of materials.
 * - Works out the hovered cell every frame (cursor ray vs. deployed entities and the build
 *   layer's plane) and shows hover / selection markers. Clicking is handled by the player
 *   controller, which selects the hovered cell.
 *
 * Uses only engine content (/Engine/BasicShapes), so it works before the project has any art.
 */
UCLASS(Config = Game)
class LOOTBOXRECURSION_API ALRWorldGridActor : public AActor
{
	GENERATED_BODY()

public:
	ALRWorldGridActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Grid-space position (cm, relative to this actor) of a cell's floor centre. */
	FVector CellToLocal(const FIntVector& Cell) const;

	/** Distance between neighbouring cell centres (across a hexagon's flat sides), and between layers, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	float CellSize = 100.f;

	/**
	 * Optional material for the grid beams, outlines included. If the asset exists it replaces
	 * the built-in flat unlit look: the grid sets its "Color" vector parameter (HDR, so values
	 * above 1 bloom). Make it Unlit with Color driving Emissive, and tick "Used with Instanced
	 * Static Meshes". See docs/ROADMAP.md, M1.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "World Grid")
	FSoftObjectPath BeamMaterialPath = FSoftObjectPath(TEXT("/Game/Materials/M_GridBeam.M_GridBeam"));

	/** Rings of cells drawn around the patch centre (the lines fade out before the last one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	int32 TileRadius = 32;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleWorldChanged();

	UFUNCTION()
	void HandleMatterChanged();

private:
	UStaticMeshComponent* CreateMesh(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial, bool bTraceable);
	/** A cell entity's shape (or a part of one) in Color. Below Opacity 1 it draws see-through and unlit. */
	UStaticMeshComponent* CreateEntityMesh(UStaticMesh* Mesh, const FLinearColor& Color, float Opacity, bool bTraceable);
	UInstancedStaticMeshComponent* CreateBeamLayer(const FLinearColor& Tint);
	FIntVector GetFocusCell() const;
	void BuildTilePattern();
	void BuildOutline(UInstancedStaticMeshComponent* Outline, float Width);
	void RebuildLines();
	void UpdateLineWidth();
	void MoveTiles(const FIntVector& Anchor);
	void UpdateHover();
	void UpdateMarkers();
	void RebuildEntities();
	/** Create, update or remove the gas disc of every cell whose matter changed. */
	void UpdateMatter();
	void FaceLabelsToCamera();

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	/** Overdensities (ripples in the pocket universe) are drawn as spheres. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	/** Lit, with a "Color" parameter: entities and the placement ghost. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Unlit (see LRUnlit): grid beams and cell outlines. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitMaterial;

	/** Unlit translucent (one-sided if the engine has it): see-through entities, e.g. ripples and irradiators. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> TranslucentMaterial;

	/** BeamMaterialPath, if that asset exists; used instead of UnlitMaterial for the beams. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> WhiteTexture;

	/** Grid beams, one layer per brightness step (dimmest first). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TileLayers;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> HoverOutline;

	/** A ghost over the hovered cell (hidden for now; meant for a build preview). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HoverMarker;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HoverMaterial;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> SelectionOutline;

	/** Deployed entity visuals; EntityCells[i] is the cell of EntityMeshes[i] / EntityLabels[i]. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EntityMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> EntityLabels;

	/** Decorations that aren't hover targets (e.g. the cache and radiation source inside an irradiator). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EntityExtras;

	TArray<FIntVector> EntityCells;

	/**
	 * Gas discs by cell, kept and updated in place (matter changes every few seconds). Not a
	 * UPROPERTY: the actor owns its components, which keeps them alive.
	 */
	TMap<FIntVector, TWeakObjectPtr<UStaticMeshComponent>> MatterMeshes;

	/** The cell the tile patch is currently centred on. */
	FIntVector TileAnchor = FIntVector::ZeroValue;
	bool bTilesPlaced = false;

	/** Beam width multiplier for the current zoom (a power of LineWidthStep). */
	float LineWidthScale = 1.f;
};
