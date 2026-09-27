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

/**
 * 3D view of the pocket universe's build grid. Every cell is an integer (X, Y, Z); Z is the
 * build layer. Rails/Vue equivalent: WorldGrid.vue + WorldCellSlot.vue, now in three
 * dimensions.
 *
 * - Draws a patch of floor tiles for the current build layer around the camera focus, using
 *   instanced meshes (one draw call per tile colour), with ruler lines every 4/16 cells.
 *   The patch is built once and only moved, in whole ruler periods, as the camera pans.
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

	/** Floor tiles drawn in each direction around the patch centre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	int32 TileRadius = 24;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleWorldChanged();

private:
	UStaticMeshComponent* CreateMesh(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial, bool bTraceable);
	UInstancedStaticMeshComponent* CreateTileLayer(const FLinearColor& Color);
	FIntVector GetFocusCell() const;
	void BuildTilePattern();
	void MoveTiles(const FIntVector& Anchor);
	void UpdateHover();
	void UpdateMarkers();
	void RebuildEntities();
	void FaceLabelsToCamera();

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Floor tiles: plain, minor ruler line (every 4), major ruler line (every 16). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TileLayers;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HoverMarker;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HoverMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SelectionMarker;

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
};
