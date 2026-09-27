#include "Game/LRWorldLineActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/LRGameSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// BasicShapeMaterial exposes a "Color" vector parameter.
	const FName ColorParam(TEXT("Color"));

	// Palette from the Rails app's application.css (--color-fac-*).
	const FLinearColor TileColor = FLinearColor(FColor::FromHex(TEXT("2D2719")));
	const FLinearColor TileMinorColor = FLinearColor(FColor::FromHex(TEXT("4A3F2F")));
	const FLinearColor TileMajorColor = FLinearColor(FColor::FromHex(TEXT("8B7355")));
	const FLinearColor TileSelectedColor = FLinearColor(FColor::FromHex(TEXT("E8A020")));
	const FColor LabelColor = FColor::FromHex(TEXT("8B7A5E"));
	const FColor EntityLabelColor = FColor::FromHex(TEXT("D4C5A0"));
}

ALRWorldLineActor::ALRWorldLineActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// ConstructorHelpers may only be used in constructors. These are engine assets that
	// ship with every Unreal install, so the project needs no art to run.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	BaseMaterial = MaterialFinder.Object;
}

FVector ALRWorldLineActor::GetCellWorldLocation(float Coordinate) const
{
	return GetActorTransform().TransformPosition(FVector(0.f, Coordinate * CellSpacing, 0.f));
}

void ALRWorldLineActor::BeginPlay()
{
	Super::BeginPlay();

	const int32 TileCount = HalfWindow * 2 + 1;
	for (int32 Index = 0; Index < TileCount; ++Index)
	{
		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Tile = CreateMeshComponent(CubeMesh, Material);
		// The cube is 100cm; make a flat slab slightly narrower than the cell spacing.
		Tile->SetRelativeScale3D(FVector(1.2f, CellSpacing / 100.f * 0.9f, 0.12f));
		Tiles.Add(Tile);
		TileMaterials.Add(Material);

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(RootComponent);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(36.f);
		Label->SetTextRenderColor(LabelColor);
		Label->RegisterComponent();
		TileLabels.Add(Label);
	}

	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.AddDynamic(this, &ALRWorldLineActor::HandleWorldChanged);
		LayoutTiles(Subsystem->GetFocusCoordinate() - HalfWindow);
	}
	RebuildEntities();
}

void ALRWorldLineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.RemoveDynamic(this, &ALRWorldLineActor::HandleWorldChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ALRWorldLineActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		const int32 NewWindowStart = Subsystem->GetFocusCoordinate() - HalfWindow;
		if (NewWindowStart != WindowStart)
		{
			LayoutTiles(NewWindowStart);
		}
	}
	UpdateTileColors();
}

UStaticMeshComponent* ALRWorldLineActor::CreateMeshComponent(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial)
{
	// Runtime-created components: NewObject + attach + RegisterComponent.
	// (CreateDefaultSubobject is only for components that exist on every instance from construction.)
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Component->SetCollisionResponseToAllChannels(ECR_Block); // clicks trace on the Visibility channel
	Component->OnClicked.AddDynamic(this, &ALRWorldLineActor::HandleComponentClicked);
	Component->RegisterComponent();

	OutMaterial = BaseMaterial ? UMaterialInstanceDynamic::Create(BaseMaterial, this) : nullptr;
	if (OutMaterial)
	{
		Component->SetMaterial(0, OutMaterial);
	}
	return Component;
}

void ALRWorldLineActor::LayoutTiles(int32 NewWindowStart)
{
	WindowStart = NewWindowStart;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		const int32 Coordinate = WindowStart + Index;
		const float Y = Coordinate * CellSpacing;
		Tiles[Index]->SetRelativeLocation(FVector(0.f, Y, 0.f));

		// Labels sit on the camera side (-X) of the tile, facing the camera.
		TileLabels[Index]->SetRelativeLocation(FVector(-90.f, Y, 12.f));
		TileLabels[Index]->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		TileLabels[Index]->SetText(FText::AsNumber(Coordinate));
	}
}

void ALRWorldLineActor::UpdateTileColors()
{
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	int32 SelectedCell = 0;
	const bool bHasSelection = Subsystem && Subsystem->GetSelectedCell(SelectedCell);

	for (int32 Index = 0; Index < TileMaterials.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = TileMaterials[Index];
		if (!Material)
		{
			continue;
		}
		const int32 Coordinate = WindowStart + Index;
		// Same ruler rhythm as WorldGrid.vue: major tick every 16, minor every 4.
		FLinearColor Color = TileColor;
		if (bHasSelection && Coordinate == SelectedCell) { Color = TileSelectedColor; }
		else if (Coordinate % 16 == 0)                   { Color = TileMajorColor; }
		else if (Coordinate % 4 == 0)                    { Color = TileMinorColor; }
		Material->SetVectorParameterValue(ColorParam, Color);
	}
}

void ALRWorldLineActor::HandleWorldChanged()
{
	RebuildEntities();
}

void ALRWorldLineActor::RebuildEntities()
{
	for (const TPair<int32, TObjectPtr<UStaticMeshComponent>>& Pair : EntityMeshes)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	for (const TPair<int32, TObjectPtr<UTextRenderComponent>>& Pair : EntityLabels)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	EntityMeshes.Reset();
	EntityLabels.Reset();

	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const FLRSimulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
	if (!Simulation)
	{
		return;
	}

	for (const TPair<int32, FLRPlacedEntity>& Pair : Simulation->GetPlaced())
	{
		const FLRPlacedEntity& Entity = Pair.Value;
		const FLRItemDef* Def = Simulation->GetData().FindItem(Entity.Item);
		const float Y = Entity.Coordinate * CellSpacing;

		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Mesh = CreateMeshComponent(CubeMesh, Material);
		Mesh->SetRelativeLocation(FVector(0.f, Y, 60.f));
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.0f));
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParam, Def ? Def->GetLinearColor() : FLinearColor::Gray);
		}
		EntityMeshes.Add(Entity.Coordinate, Mesh);

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(RootComponent);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(28.f);
		Label->SetTextRenderColor(EntityLabelColor);
		Label->SetRelativeLocation(FVector(0.f, Y, 150.f));
		Label->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		Label->SetText(FText::FromString(Def ? Def->Abbrev : Entity.Item.ToString()));
		Label->RegisterComponent();
		EntityLabels.Add(Entity.Coordinate, Label);
	}
}

void ALRWorldLineActor::HandleComponentClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (Tiles[Index].Get() == TouchedComponent)
		{
			Subsystem->SelectCell(WindowStart + Index);
			return;
		}
	}
	for (const TPair<int32, TObjectPtr<UStaticMeshComponent>>& Pair : EntityMeshes)
	{
		if (Pair.Value.Get() == TouchedComponent)
		{
			Subsystem->SelectCell(Pair.Key);
			return;
		}
	}
}
