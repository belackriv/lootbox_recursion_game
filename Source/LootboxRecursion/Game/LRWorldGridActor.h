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
 * - Draws a window of floor tiles for the current build layer around the camera focus,
 *   using instanced meshes (one draw call per tile colour), with ruler lines every 4/16 cells.
 * - Draws one mesh per deployed entity, on every layer.
 * - Traces the mouse cursor every frame to find the hovered cell, shows a hover marker and
 *   a selection marker, and selects the hovered cell on click.
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

	/** Floor tiles drawn in each direction around the camera focus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Grid")
	int32 TileRadius = 12;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleWorldChanged();

	UFUNCTION()
	void HandleComponentClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

private:
	UStaticMeshComponent* CreateMesh(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial, bool bClickable);
	UInstancedStaticMeshComponent* CreateTileLayer(const FLinearColor& Color);
	FIntVector GetFocusCell() const;
	void RebuildTiles(const FIntVector& Center);
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

	TArray<FIntVector> EntityCells;

	FIntVector TileCenter = FIntVector(TNumericLimits<int32>::Max(), 0, 0);
};
