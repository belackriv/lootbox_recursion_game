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
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Rendering/LRUnlit.h"
#include "Simulation/LRHexGrid.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// BasicShapeMaterial exposes a "Color" vector parameter.
	const FName ColorParam(TEXT("Color"));

	// The grid is thin beams of light in the void. Tints above 1 are HDR and glow through bloom.
	const FLinearColor LineColor(0.85f, 0.92f, 1.f);
	const FLinearColor SelectedColor(FColor::FromHex(TEXT("F2A93B"))); // accretion-disk amber

	FLinearColor Glow(const FLinearColor& Color, float Intensity)
	{
		return FLinearColor(Color.R * Intensity, Color.G * Intensity, Color.B * Intensity, 1.f);
	}

	constexpr float LineBrightness = 0.7f;
	/** Beam widths as a fraction of a cell, before scaling up with camera distance. */
	constexpr float LineWidth = 0.02f;
	constexpr float HoverWidth = 0.04f;
	constexpr float SelectionWidth = 0.05f;

	/** Fraction of the patch radius where the fade-out starts. */
	constexpr float FadeStart = 0.45f;

	/** Brightness steps of the beam layers (one instanced component, one draw call each). */
	constexpr int32 BrightnessSteps = 8;
	constexpr float DimmestStep = 0.08f;
	constexpr float StepRatio = 1.6f;

	float StepBrightness(int32 Step)
	{
		return DimmestStep * FMath::Pow(StepRatio, static_cast<float>(Step));
	}

	/** The beam layer closest to a brightness, or INDEX_NONE if it's too dim to draw. */
	int32 StepForBrightness(float Brightness)
	{
		if (Brightness < DimmestStep * 0.7f)
		{
			return INDEX_NONE;
		}
		const int32 Step = FMath::RoundToInt(FMath::Loge(Brightness / DimmestStep) / FMath::Loge(StepRatio));
		return FMath::Clamp(Step, 0, BrightnessSteps - 1);
	}

	/** Camera distance (cm) at which beams have their base width; they widen beyond it. */
	constexpr float LineWidthReferenceDistance = 1500.f;
	constexpr float LineWidthStep = 1.3f;
	const FColor EntityLabelColor = FColor::FromHex(TEXT("D6DCE8"));

	/** The patch re-centres on the focus once the camera has panned this many cells from its centre. */
	constexpr int32 PatchRecentreCells = 4;

	/** A beam along one hexagon edge: from corner Index to corner Index + 1, around Centre. */
	FTransform EdgeBeam(const FVector& Centre, int32 Index, float Spacing, float Thickness)
	{
		const FVector From = Centre + FLRHexGrid::Corner(Index, Spacing);
		const FVector To = Centre + FLRHexGrid::Corner(Index + 1, Spacing);
		// The engine cube is 100cm; overlap the corners by one beam width.
		const float Length = static_cast<float>(FLRHexGrid::CornerRadius(Spacing)) / 100.f + Thickness;
		return FTransform((To - From).Rotation(), 0.5 * (From + To), FVector(Length, Thickness, Thickness));
	}
}

ALRWorldGridActor::ALRWorldGridActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// ConstructorHelpers may only be used in constructors. These are engine assets that
	// ship with every Unreal install, so the project needs no art to run.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(LRUnlit::OpaqueMaterialPath);
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	BaseMaterial = MaterialFinder.Object;
	UnlitMaterial = UnlitFinder.Object;
}

FVector ALRWorldGridActor::CellToLocal(const FIntVector& Cell) const
{
	return FLRHexGrid::ToLocal(Cell, CellSize);
}

void ALRWorldGridActor::BeginPlay()
{
	Super::BeginPlay();

	WhiteTexture = LRUnlit::MakeSolidTexture(FColor::White);
	// A project material for the beams, if one has been made (checked first so a missing
	// asset doesn't log a load failure every time).
	if (!BeamMaterialPath.IsNull() && FPackageName::DoesPackageExist(BeamMaterialPath.GetLongPackageName()))
	{
		BeamMaterial = Cast<UMaterialInterface>(BeamMaterialPath.TryLoad());
	}
	for (int32 Step = 0; Step < BrightnessSteps; ++Step)
	{
		TileLayers.Add(CreateBeamLayer(Glow(LineColor, StepBrightness(Step))));
	}
	HoverOutline = CreateBeamLayer(Glow(LineColor, 1.4f));
	HoverOutline->SetVisibility(false);
	SelectionOutline = CreateBeamLayer(Glow(SelectedColor, 3.f));
	SelectionOutline->SetVisibility(false);
	RebuildLines();

	UMaterialInstanceDynamic* HoverMat = nullptr;
	HoverMarker = CreateMesh(CubeMesh, HoverMat, /*bTraceable*/ false);
	HoverMaterial = HoverMat;
	HoverMarker->SetVisibility(false);

	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.AddDynamic(this, &ALRWorldGridActor::HandleWorldChanged);
		Subsystem->OnMatterChanged.AddDynamic(this, &ALRWorldGridActor::HandleMatterChanged);
	}
	RebuildEntities();
	UpdateMatter();
}

void ALRWorldGridActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnWorldChanged.RemoveDynamic(this, &ALRWorldGridActor::HandleWorldChanged);
		Subsystem->OnMatterChanged.RemoveDynamic(this, &ALRWorldGridActor::HandleMatterChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ALRWorldGridActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLineWidth();

	// Any whole-cell shift maps the hexagon pattern onto itself, so the patch just follows the
	// focus a few cells at a time: moving a few components now and then is cheap, rebuilding
	// thousands of instances every time the camera crosses a cell is not.
	const FIntVector Focus = GetFocusCell();
	if (!bTilesPlaced || Focus.Z != TileAnchor.Z || FLRHexGrid::Distance(Focus, TileAnchor) > PatchRecentreCells)
	{
		MoveTiles(Focus);
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

UInstancedStaticMeshComponent* ALRWorldGridActor::CreateBeamLayer(const FLinearColor& Tint)
{
	// Instanced, so thousands of beams are one draw call. Unlit, so they glow in the dark
	// instead of being shaded by the lights. No collision: the hovered cell is found by
	// intersecting the cursor ray with the layer's plane.
	UInstancedStaticMeshComponent* Beams = NewObject<UInstancedStaticMeshComponent>(this);
	Beams->SetStaticMesh(CubeMesh);
	Beams->SetupAttachment(RootComponent);
	Beams->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LRUnlit::ExcludeFromLighting(Beams);
	Beams->RegisterComponent();
	if (BeamMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BeamMaterial, this);
		Material->SetVectorParameterValue(ColorParam, Tint);
		Beams->SetMaterial(0, Material);
	}
	else if (UMaterialInstanceDynamic* Material = LRUnlit::MakeMaterial(UnlitMaterial, this, WhiteTexture, Tint))
	{
		Beams->SetMaterial(0, Material);
	}
	return Beams;
}

// ---- Grid lines -----------------------------------------------------------------------

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
	return FLRHexGrid::FromLocal(Local, CellSize, Layer);
}

void ALRWorldGridActor::BuildTilePattern()
{
	// Beams on the hexagons' edges; the cells themselves are empty, so the cosmos shows through.
	// Each cell draws three of its six edges (its neighbours draw the other three). Each edge
	// fades out towards the edge of the patch and goes into the beam layer closest to its
	// brightness.
	const float Thickness = LineWidth * (CellSize / 100.f) * LineWidthScale; // the engine cube is 100cm
	// Fade out by the patch's inscribed circle (the outer ring's flat sides are sqrt(3)/2 as far
	// as its corners), so the patch's own hexagonal outline never shows.
	const float PatchRadius = FMath::Max(TileRadius, 1) * CellSize * 0.85f;

	TArray<TArray<FTransform>> PerLayer;
	PerLayer.SetNum(TileLayers.Num());
	for (const FIntVector& Cell : FLRHexGrid::CellsInRadius(FIntVector::ZeroValue, TileRadius))
	{
		const FVector Centre = FLRHexGrid::ToLocal(Cell, CellSize);
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			const FTransform Beam = EdgeBeam(Centre, Edge, CellSize, Thickness);
			const float Distance = static_cast<float>(Beam.GetLocation().Size2D()) / PatchRadius;
			const float Brightness = LineBrightness * (1.f - FMath::SmoothStep(FadeStart, 1.f, Distance));
			const int32 Layer = StepForBrightness(Brightness);
			if (PerLayer.IsValidIndex(Layer))
			{
				PerLayer[Layer].Add(Beam);
			}
		}
	}

	for (int32 Layer = 0; Layer < TileLayers.Num(); ++Layer)
	{
		TileLayers[Layer]->ClearInstances();
		TileLayers[Layer]->AddInstances(PerLayer[Layer], /*bShouldReturnIndices*/ false);
	}
}

void ALRWorldGridActor::BuildOutline(UInstancedStaticMeshComponent* Outline, float Width)
{
	// Six beams around one hexagon, centred on the component (which sits on the cell's centre).
	const float Thickness = Width * (CellSize / 100.f) * LineWidthScale;
	TArray<FTransform> Beams;
	for (int32 Edge = 0; Edge < 6; ++Edge)
	{
		Beams.Add(EdgeBeam(FVector::ZeroVector, Edge, CellSize, Thickness));
	}
	Outline->ClearInstances();
	Outline->AddInstances(Beams, /*bShouldReturnIndices*/ false);
}

void ALRWorldGridActor::RebuildLines()
{
	BuildTilePattern();
	BuildOutline(HoverOutline, HoverWidth);
	BuildOutline(SelectionOutline, SelectionWidth);
}

void ALRWorldGridActor::UpdateLineWidth()
{
	// Keep beams a pixel or two wide at any zoom: widen them as the camera pulls back, in
	// coarse steps so zooming only rebuilds the pattern now and then.
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!CameraManager || !Pawn)
	{
		return;
	}
	const float Distance = FVector::Dist(CameraManager->GetCameraLocation(), Pawn->GetActorLocation());
	const float Wanted = FMath::Max(1.f, Distance / LineWidthReferenceDistance);
	const float Stepped = FMath::Pow(LineWidthStep, FMath::RoundToFloat(FMath::Loge(Wanted) / FMath::Loge(LineWidthStep)));
	if (!FMath::IsNearlyEqual(Stepped, LineWidthScale, 0.01f))
	{
		LineWidthScale = Stepped;
		RebuildLines();
	}
}

void ALRWorldGridActor::MoveTiles(const FIntVector& Anchor)
{
	TileAnchor = Anchor;
	bTilesPlaced = true;
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
			PlaneCell = FLRHexGrid::FromLocal(Point, CellSize, Subsystem->GetBuildLayer());
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

	FIntVector Selected;
	const bool bHasSelection = Subsystem->GetSelectedCell(Selected);
	SelectionOutline->SetVisibility(bHasSelection);
	if (bHasSelection)
	{
		SelectionOutline->SetRelativeLocation(CellToLocal(Selected));
	}

	FIntVector Hovered;
	const bool bHasHover = Subsystem->GetHoveredCell(Hovered);
	HoverOutline->SetVisibility(bHasHover);
	HoverMarker->SetVisibility(false);
	if (!bHasHover)
	{
		return;
	}
	HoverOutline->SetRelativeLocation(CellToLocal(Hovered));
}

// ---- Deployed entities ----------------------------------------------------------------

void ALRWorldGridActor::HandleWorldChanged()
{
	RebuildEntities();
}

void ALRWorldGridActor::HandleMatterChanged()
{
	UpdateMatter();
}

void ALRWorldGridActor::UpdateMatter()
{
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const FLRSimulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
	if (!Simulation || !SphereMesh)
	{
		return;
	}
	const TMap<FIntVector, FLRCellMatter>& Matter = Simulation->GetAllMatter();

	// Cells that no longer hold matter lose their disc.
	for (auto It = MatterMeshes.CreateIterator(); It; ++It)
	{
		if (!Matter.Contains(It.Key()) || !It.Value().IsValid())
		{
			if (UStaticMeshComponent* Stale = It.Value().Get())
			{
				Stale->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}

	const float Scale = CellSize / 100.f;
	for (const TPair<FIntVector, FLRCellMatter>& Pair : Matter)
	{
		// Tint: the materials' colours weighted by amount. Size: grows with the log of the total,
		// from a small puff to nearly the whole hexagon.
		FLinearColor Tint = FLinearColor::Black;
		const int32 Total = Pair.Value.Total();
		for (const FLRItemAmount& Amount : Pair.Value.Amounts)
		{
			const FLRItemDef* Def = Simulation->GetData().FindItem(Amount.Item);
			Tint += (Def ? Def->GetLinearColor() : FLinearColor::Gray) * (static_cast<float>(Amount.Count) / FMath::Max(Total, 1));
		}
		Tint.A = 1.f;
		const float Size = FMath::Clamp(0.25f + 0.12f * FMath::Log2(1.f + Total / 10.f), 0.25f, 0.9f);

		UStaticMeshComponent* Disc = MatterMeshes.FindRef(Pair.Key).Get();
		if (!Disc)
		{
			UMaterialInstanceDynamic* NewMaterial = nullptr;
			Disc = CreateMesh(SphereMesh, NewMaterial, /*bTraceable*/ false);
			MatterMeshes.Add(Pair.Key, Disc);
		}
		Disc->SetRelativeLocation(CellToLocal(Pair.Key) + FVector(0.f, 0.f, 3.f * Scale));
		Disc->SetRelativeScale3D(FVector(Size * Scale, Size * Scale, 0.06f * Scale));
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(ColorParam, Tint);
		}
	}
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
	for (const TObjectPtr<UStaticMeshComponent>& Extra : EntityExtras)
	{
		if (Extra)
		{
			Extra->DestroyComponent();
		}
	}
	EntityMeshes.Reset();
	EntityLabels.Reset();
	EntityExtras.Reset();
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

		// Shapes, as fractions of a cell (the engine cube and sphere are 100cm across, pivot at the
		// centre), each sitting on the cell floor:
		// - overdensities (ripples) are spheres that swell with their amplitude,
		// - caches are square prisms, narrow and tall,
		// - everything else (machines) is a cube that fits inside the hexagon (its diagonal is
		//   under the hexagon's flat-to-flat width).
		const bool bOverdensity = Def && Def->IsOverdensity() && SphereMesh;
		const bool bCache = Def && Def->IsLootBox();
		FVector Size(0.65f);
		if (bOverdensity)
		{
			Size = FVector(0.35f + 0.55f * FMath::Clamp(static_cast<float>(Entity.Amplitude) / FMath::Max(1, Def->MaxAmplitude), 0.f, 1.f));
		}
		else if (bCache)
		{
			Size = FVector(0.4f, 0.4f, 0.8f);
		}
		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Mesh = CreateMesh(bOverdensity ? SphereMesh.Get() : CubeMesh.Get(), Material, /*bTraceable*/ true);
		Mesh->SetRelativeLocation(Floor + FVector(0.f, 0.f, 50.f * Size.Z * Scale));
		Mesh->SetRelativeScale3D(Size * Scale);
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParam, Def ? Def->GetLinearColor() : FLinearColor::Gray);
		}

		// Irradiation enclosures: a small cube in the radiation's colour on top when a source is
		// loaded, and chamber progress in the label. Overdensities show their amplitude.
		FString LabelText = Def ? Def->Abbrev : Entity.Item.ToString();
		if (bOverdensity)
		{
			LabelText += FString::Printf(TEXT(" (%d)"), Entity.Amplitude);
		}
		if (Def && Def->IsEnclosure())
		{
			const FLRItemDef* SourceDef = Entity.Source.IsEmpty() ? nullptr : Simulation->GetData().FindItem(Entity.Source.Item);
			const FLRRadiationDef* Radiation = SourceDef ? Simulation->GetData().FindRadiation(SourceDef->Radiation) : nullptr;
			if (SourceDef)
			{
				UMaterialInstanceDynamic* SourceMaterial = nullptr;
				UStaticMeshComponent* SourceMesh = CreateMesh(CubeMesh, SourceMaterial, /*bTraceable*/ false);
				SourceMesh->SetRelativeLocation(Floor + FVector(0.f, 0.f, 85.f * Scale));
				SourceMesh->SetRelativeScale3D(FVector(0.2f * Scale));
				if (SourceMaterial)
				{
					SourceMaterial->SetVectorParameterValue(ColorParam, Radiation ? Radiation->GetLinearColor() : SourceDef->GetLinearColor());
				}
				EntityExtras.Add(SourceMesh);
			}
			if (const FLRLootBoxInstance* Box = Entity.Chamber.IsEmpty() ? nullptr : Simulation->FindLootBox(Entity.Chamber.InstanceId))
			{
				LabelText += Box->bRevealed
					? TEXT(" [cache: observed]")
					: FString::Printf(TEXT(" [cache %d/%d]"), Box->Modifiers.Num(), Def->MaxExposureStacks);
			}
		}

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(RootComponent);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(28.f * Scale);
		Label->SetTextRenderColor(EntityLabelColor);
		Label->SetRelativeLocation(Floor + FVector(0.f, 0.f, 100.f * Scale));
		Label->SetText(FText::FromString(LabelText));
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
