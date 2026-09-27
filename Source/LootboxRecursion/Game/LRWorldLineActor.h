#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LRWorldLineActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * 3D view of the one-dimensional world: a row of cells along the +Y axis with deployed
 * entities standing on them. Rails/Vue equivalent: WorldGrid.vue + WorldCellSlot.vue.
 *
 * The world is unbounded, so like the Vue grid this is "virtualized": a fixed pool of tile
 * components is re-labelled as the camera focus moves. Deployed entities get one mesh each.
 *
 * Uses only engine content (/Engine/BasicShapes), so it works before the project has any art.
 */
UCLASS()
class LOOTBOXRECURSION_API ALRWorldLineActor : public AActor
{
	GENERATED_BODY()

public:
	ALRWorldLineActor();

	virtual void Tick(float DeltaSeconds) override;

	/** World-space centre of a cell (fractional coordinates are fine). */
	FVector GetCellWorldLocation(float Coordinate) const;

	/** Distance between cell centres, in Unreal units (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Line")
	float CellSpacing = 150.f;

	/** Tiles drawn either side of the focus coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Line")
	int32 HalfWindow = 14;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleWorldChanged();

	UFUNCTION()
	void HandleComponentClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

private:
	UStaticMeshComponent* CreateMeshComponent(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial);
	void LayoutTiles(int32 NewWindowStart);
	void UpdateTileColors();
	void RebuildEntities();

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TileMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> TileLabels;

	/** Deployed entity visuals keyed by world coordinate. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UStaticMeshComponent>> EntityMeshes;

	UPROPERTY()
	TMap<int32, TObjectPtr<UTextRenderComponent>> EntityLabels;

	int32 WindowStart = TNumericLimits<int32>::Min();
};
