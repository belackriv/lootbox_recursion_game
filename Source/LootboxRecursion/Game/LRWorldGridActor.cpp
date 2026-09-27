#include "Game/LRWorldGridActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/LRGameSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
	const FLinearColor HoverColor = FLinearColor(FColor::FromHex(TEXT("D4C5A0")));
	const FLinearColor SelectedColor = FLinearColor(FColor::FromHex(TEXT("E8A020")));
	const FColor EntityLabelColor = FColor::FromHex(TEXT("D4C5A0"));

	/** Major ruler line spacing; the tile patch moves in steps of this so the pattern lines up. */
	constexpr int32 RulerPeriod = 16;

	/** Positive modulo, so ruler lines stay regular across negative cells. */
	int32 Mod(int32 Value, int32 Divisor)
	{
		const int32 Result = Value % Divisor;
		return Result < 0 ? Result + Divisor : Result;
	}
}

ALRWorldGridActor::ALRWorldGridActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// ConstructorHelpers may only be used in constructors. These are engine assets that
	// ship with every Unreal install, so the project needs no art to run.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;
}

FVector ALRWorldGridActor::CellToLocal(const FIntVector& Cell) const
{
	return FVector(Cell.X * CellSize, Cell.Y * CellSize, Cell.Z * CellSize);
}

void ALRWorldGridActor::BeginPlay()
{
	Super::BeginPlay();

	TileLayers.Add(CreateTileLayer(TileColor));
	TileLayers.Add(CreateTileLayer(TileMinorColor));
	TileLayers.Add(CreateTileLayer(TileMajorColor));
	BuildTilePattern();

	UMaterialInstanceDynamic* SelectionMaterial = nullptr;
	SelectionMarker = CreateMesh(CubeMesh, SelectionMaterial, /*bTraceable*/ false);
	if (SelectionMaterial)
	{
		SelectionMaterial->SetVectorParameterValue(ColorParam, SelectedColor);
	}
	SelectionMarker->SetVisibility(false);

	UMaterialInstanceDynamic* HoverMat = nullptr;
	HoverMarker = CreateMesh(CubeMesh, HoverMat, /*bTraceable*/ false);
	HoverMaterial = HoverMat;
	HoverMarker->SetVisibility(false);

	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.AddDynamic(this, &ALRWorldGridActor::HandleWorldChanged);
	}
	RebuildEntities();
}

void ALRWorldGridActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.RemoveDynamic(this, &ALRWorldGridActor::HandleWorldChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ALRWorldGridActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Snap the patch to whole ruler periods: moving 3 components now and then is cheap,
	// rebuilding ~2500 instances every time the camera crosses a cell is not.
	const FIntVector Focus = GetFocusCell();
	const FIntVector Anchor(
		FMath::RoundToInt(static_cast<float>(Focus.X) / RulerPeriod) * RulerPeriod,
		FMath::RoundToInt(static_cast<float>(Focus.Y) / RulerPeriod) * RulerPeriod,
		Focus.Z);
	if (Anchor != TileAnchor)
	{
		MoveTiles(Anchor);
	}
	UpdateHover();
	UpdateMarkers();
	FaceLabelsToCamera();
}

// ---- Construction helpers -------------------------------------------------------------

UStaticMeshComponent* ALRWorldGridActor::CreateMesh(UStaticMesh* Mesh, UMaterialInstanceDynamic*& OutMaterial, bool bTraceable)
{
	// Runtime-created components: NewObject + attach + RegisterComponent.
	// (CreateDefaultSubobject is only for components that exist on every instance from construction.)
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	if (bTraceable)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionResponseToAllChannels(ECR_Block); // the cursor traces the Visibility channel
	}
	else
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
	}
	Component->RegisterComponent();

	OutMaterial = BaseMaterial ? UMaterialInstanceDynamic::Create(BaseMaterial, this) : nullptr;
	if (OutMaterial)
	{
		Component->SetMaterial(0, OutMaterial);
	}
	return Component;
}

UInstancedStaticMeshComponent* ALRWorldGridActor::CreateTileLayer(const FLinearColor& Color)
{
	// One instanced component per colour: thousands of tiles, one draw call each. No collision:
	// the hovered cell is found by intersecting the cursor ray with the layer's plane.
	UInstancedStaticMeshComponent* Tiles = NewObject<UInstancedStaticMeshComponent>(this);
	Tiles->SetStaticMesh(CubeMesh);
	Tiles->SetupAttachment(RootComponent);
	Tiles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tiles->SetCastShadow(false);
	Tiles->RegisterComponent();

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			Material->SetVectorParameterValue(ColorParam, Color);
			Tiles->SetMaterial(0, Material);
		}
	}
	return Tiles;
}

// ---- Floor tiles ----------------------------------------------------------------------

FIntVector ALRWorldGridActor::GetFocusCell() const
{
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const int32 Layer = Subsystem ? Subsystem->GetBuildLayer() : 0;

	// The camera pawn sits on the point it looks at.
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn)
	{
		return FIntVector(0, 0, Layer);
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	return FIntVector(FMath::RoundToInt(Local.X / CellSize), FMath::RoundToInt(Local.Y / CellSize), Layer);
}

void ALRWorldGridActor::BuildTilePattern()
{
	const float TileScale = CellSize / 100.f; // the engine cube is 100cm
	const FVector Scale(0.94f * TileScale, 0.94f * TileScale, 0.04f * TileScale);

	TArray<FTransform> PerKind[3];
	for (int32 X = -TileRadius; X <= TileRadius; ++X)
	{
		for (int32 Y = -TileRadius; Y <= TileRadius; ++Y)
		{
			// Ruler rhythm from WorldGrid.vue, now on both axes: major every 16, minor every 4.
			// The patch only ever moves by whole periods, so local and world ruler lines agree.
			int32 Kind = 0;
			if (Mod(X, RulerPeriod) == 0 || Mod(Y, RulerPeriod) == 0) { Kind = 2; }
			else if (Mod(X, 4) == 0 || Mod(Y, 4) == 0)                { Kind = 1; }

			// Tile top sits exactly on the layer's floor.
			const FVector Location(X * CellSize, Y * CellSize, -2.f * TileScale);
			PerKind[Kind].Add(FTransform(FRotator::ZeroRotator, Location, Scale));
		}
	}

	for (int32 Kind = 0; Kind < TileLayers.Num() && Kind < 3; ++Kind)
	{
		TileLayers[Kind]->ClearInstances();
		TileLayers[Kind]->AddInstances(PerKind[Kind], /*bShouldReturnIndices*/ false);
	}
}

void ALRWorldGridActor::MoveTiles(const FIntVector& Anchor)
{
	TileAnchor = Anchor;
	for (const TObjectPtr<UInstancedStaticMeshComponent>& Tiles : TileLayers)
	{
		Tiles->SetRelativeLocation(CellToLocal(Anchor));
	}
}

// ---- Hover / selection ----------------------------------------------------------------

void ALRWorldGridActor::UpdateHover()
{
	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector RayOrigin;
	FVector RayDirection;
	if (!Subsystem || !Controller || !Controller->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		if (Subsystem)
		{
			Subsystem->ClearHoveredCell();
		}
		return;
	}

	// 1. Deployed entities (the only things here with collision), on any layer.
	bool bHitEntity = false;
	float EntityDistance = TNumericLimits<float>::Max();
	FIntVector EntityCell;
	FHitResult Hit;
	if (Controller->GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Hit))
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		for (int32 Index = 0; Index < EntityMeshes.Num(); ++Index)
		{
			if (EntityMeshes[Index].Get() == HitComponent)
			{
				bHitEntity = true;
				EntityDistance = Hit.Distance;
				EntityCell = EntityCells[Index];
				break;
			}
		}
	}

	// 2. The build layer's floor plane, done with maths instead of collision.
	const FTransform& GridTransform = GetActorTransform();
	const FVector LocalOrigin = GridTransform.InverseTransformPosition(RayOrigin);
	const FVector LocalDirection = GridTransform.InverseTransformVectorNoScale(RayDirection);
	const float PlaneZ = Subsystem->GetBuildLayer() * CellSize;
	bool bHitPlane = false;
	float PlaneDistance = TNumericLimits<float>::Max();
	FIntVector PlaneCell;
	if (FMath::Abs(LocalDirection.Z) > UE_KINDA_SMALL_NUMBER)
	{
		const float RayT = (PlaneZ - LocalOrigin.Z) / LocalDirection.Z;
		if (RayT > 0.f)
		{
			const FVector Point = LocalOrigin + LocalDirection * RayT;
			bHitPlane = true;
			PlaneDistance = RayT;
			PlaneCell = FIntVector(FMath::RoundToInt(Point.X / CellSize), FMath::RoundToInt(Point.Y / CellSize), Subsystem->GetBuildLayer());
		}
	}

	if (bHitEntity && (!bHitPlane || EntityDistance <= PlaneDistance))
	{
		Subsystem->SetHoveredCell(EntityCell);
	}
	else if (bHitPlane)
	{
		Subsystem->SetHoveredCell(PlaneCell);
	}
	else
	{
		Subsystem->ClearHoveredCell();
	}
}

void ALRWorldGridActor::UpdateMarkers()
{
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const FLRSimulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
	if (!Simulation)
	{
		return;
	}
	const float Scale = CellSize / 100.f;

	FIntVector Selected;
	const bool bHasSelection = Subsystem->GetSelectedCell(Selected);
	SelectionMarker->SetVisibility(bHasSelection);
	if (bHasSelection)
	{
		// A slab a little wider than the cell, so it shows around a deployed entity too.
		SelectionMarker->SetRelativeLocation(CellToLocal(Selected) + FVector(0.f, 0.f, 1.5f * Scale));
		SelectionMarker->SetRelativeScale3D(FVector(1.04f * Scale, 1.04f * Scale, 0.03f * Scale));
	}

	FIntVector Hovered;
	const bool bHasHover = Subsystem->GetHoveredCell(Hovered);
	HoverMarker->SetVisibility(bHasHover);
	if (!bHasHover)
	{
		return;
	}

	// If a deployable item is selected in the inventory and the cell is free, show a
	// small "ghost" of it; otherwise just a flat highlight.
	const FLRItemDef* Preview = nullptr;
	const int32 SlotIndex = Subsystem->GetSelectedSlot();
	if (Simulation->GetInventory().IsValidIndex(SlotIndex) && !Simulation->GetInventory()[SlotIndex].IsEmpty())
	{
		const FLRItemDef* Def = Simulation->GetData().FindItem(Simulation->GetInventory()[SlotIndex].Item);
		Preview = (Def && Def->IsPlaceable()) ? Def : nullptr;
	}

	if (Preview && !Simulation->FindPlaced(Hovered))
	{
		HoverMarker->SetRelativeLocation(CellToLocal(Hovered) + FVector(0.f, 0.f, 25.f * Scale));
		HoverMarker->SetRelativeScale3D(FVector(0.5f * Scale));
		if (HoverMaterial)
		{
			HoverMaterial->SetVectorParameterValue(ColorParam, Preview->GetLinearColor());
		}
	}
	else
	{
		HoverMarker->SetRelativeLocation(CellToLocal(Hovered) + FVector(0.f, 0.f, 3.f * Scale));
		HoverMarker->SetRelativeScale3D(FVector(0.98f * Scale, 0.98f * Scale, 0.02f * Scale));
		if (HoverMaterial)
		{
			HoverMaterial->SetVectorParameterValue(ColorParam, HoverColor);
		}
	}
}

// ---- Deployed entities ----------------------------------------------------------------

void ALRWorldGridActor::HandleWorldChanged()
{
	RebuildEntities();
}

void ALRWorldGridActor::RebuildEntities()
{
	for (const TObjectPtr<UStaticMeshComponent>& Mesh : EntityMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	for (const TObjectPtr<UTextRenderComponent>& Label : EntityLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	EntityMeshes.Reset();
	EntityLabels.Reset();
	EntityCells.Reset();

	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const FLRSimulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
	if (!Simulation)
	{
		return;
	}

	const float Scale = CellSize / 100.f;
	for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Simulation->GetPlaced())
	{
		const FLRPlacedEntity& Entity = Pair.Value;
		const FLRItemDef* Def = Simulation->GetData().FindItem(Entity.Item);
		const FVector Floor = CellToLocal(Entity.Cell);

		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Mesh = CreateMesh(CubeMesh, Material, /*bTraceable*/ true);
		Mesh->SetRelativeLocation(Floor + FVector(0.f, 0.f, 40.f * Scale));
		Mesh->SetRelativeScale3D(FVector(0.8f * Scale));
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParam, Def ? Def->GetLinearColor() : FLinearColor::Gray);
		}

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(RootComponent);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(28.f * Scale);
		Label->SetTextRenderColor(EntityLabelColor);
		Label->SetRelativeLocation(Floor + FVector(0.f, 0.f, 100.f * Scale));
		Label->SetText(FText::FromString(Def ? Def->Abbrev : Entity.Item.ToString()));
		Label->RegisterComponent();

		EntityMeshes.Add(Mesh);
		EntityLabels.Add(Label);
		EntityCells.Add(Entity.Cell);
	}
}

void ALRWorldGridActor::FaceLabelsToCamera()
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}
	const FVector CameraLocation = CameraManager->GetCameraLocation();

	// A text render reads correctly from its +X side, so point +X at the camera (yaw only).
	for (const TObjectPtr<UTextRenderComponent>& Label : EntityLabels)
	{
		if (!Label)
		{
			continue;
		}
		FVector ToCamera = CameraLocation - Label->GetComponentLocation();
		ToCamera.Z = 0.f;
		if (!ToCamera.IsNearlyZero())
		{
			Label->SetWorldRotation(ToCamera.Rotation());
		}
	}
}
