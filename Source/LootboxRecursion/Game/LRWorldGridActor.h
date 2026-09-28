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
 * 3D view of the pocket universe's build grid. Every cell is an integer (X, Y, Z); Z is the
 * build layer. Rails/Vue equivalent: WorldGrid.vue + WorldCellSlot.vue, now in three
 * dimensions.
 *
 * - Draws the current build layer's grid around the camera focus as thin, glowing beams of
 *   light (unlit instanced cubes, one draw call per brightness step), brighter every 4/16
 *   cells and fading out towards the edge. The patch is built once and only moved, in whole
 *   ruler periods, as the camera pans; it is rebuilt when zooming changes the beam width.
 * - Draws one mesh per deployed entity, on every layer.
 * - Works out the hovered cell every frame (cursor ray vs. deployed entities and the build
 *   layer's plane) and shows hover / selection markers. Clicking is handled by the player
 *   controller, which selects the hovered cell.
 *
 * Uses only engine content (/Engine/BasicShapes), so it works before the project has any art.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRWorldGridActor : public AActor
{
	GENERATED_BODY()

public:
	ALRWorldGridActor();

	virtual void Tick(float DeltaSeconds) override;

	/** Grid-space position (cm, relative to this actor) of a cell's floor centre. */
	FVector CellToLocal(const FIntVector& Cell) const;

	/** Size of one cell in Unreal units (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	float CellSize = 100.f;

	/** Grid cells drawn in each direction around the patch centre (the lines fade out before it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	int32 TileRadius = 32;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleWorldChanged();

private:
	UStaticMeshComponent* CreateMesh(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial, bool bTraceable);
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

	UPROPERTY()
	TObjectPtr<UTexture2D> WhiteTexture;

	/** Grid beams, one layer per brightness step (dimmest first). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TileLayers;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> HoverOutline;

	/** Ghost of the selected deployable item over the hovered cell. */
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

	/** Decorations that aren't hover targets (e.g. the radiation source on an enclosure). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EntityExtras;

	TArray<FIntVector> EntityCells;

	/** Where the tile patch is currently centred (a multiple of the ruler period in X/Y). */
	FIntVector TileAnchor = FIntVector(TNumericLimits<int32>::Max(), 0, 0);

	/** Beam width multiplier for the current zoom (a power of LineWidthStep). */
	float LineWidthScale = 1.f;
};
