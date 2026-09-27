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
#include "Rendering/LRUnlit.h"
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

	/** Plain line, every 4th, every 16th (the ruler rhythm from WorldGrid.vue). */
	constexpr float LineKindBrightness[] = { 0.3f, 0.7f, 2.2f };
	/** Beam widths as a fraction of a cell, before scaling up with camera distance. */
	constexpr float LineKindWidth[] = { 0.015f, 0.02f, 0.03f };
	constexpr float HoverWidth = 0.04f;
	constexpr float SelectionWidth = 0.05f;

	/** Lines are cut into segments this many cells long, so they can fade towards the patch edge. */
	constexpr float SegmentCells = 4.f;
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

	/** Major ruler line spacing; the grid patch moves in steps of this so the pattern lines up. */
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(LRUnlit::OpaqueMaterialPath);
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;
	UnlitMaterial = UnlitFinder.Object;
}

FVector ALRWorldGridActor::CellToLocal(const FIntVector& Cell) const
{
	return FVector(Cell.X * CellSize, Cell.Y * CellSize, Cell.Z * CellSize);
}

void ALRWorldGridActor::BeginPlay()
{
	Super::BeginPlay();

	WhiteTexture = LRUnlit::MakeSolidTexture(FColor::White);
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

	UpdateLineWidth();

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
	if (UMaterialInstanceDynamic* Material = LRUnlit::MakeMaterial(UnlitMaterial, this, WhiteTexture, Tint))
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
	return FIntVector(FMath::RoundToInt(Local.X / CellSize), FMath::RoundToInt(Local.Y / CellSize), Layer);
}

void ALRWorldGridActor::BuildTilePattern()
{
	// Beams on the cell boundaries; the cells themselves are empty, so the cosmos shows through.
	// Each line is cut into short segments that fade out towards the edge of the patch, and each
	// segment goes into the beam layer closest to its brightness.
	const float Scale = CellSize / 100.f; // the engine cube is 100cm
	const float Start = -(TileRadius + 0.5f); // in cells, relative to the patch centre
	const float End = TileRadius + 0.5f;

	TArray<TArray<FTransform>> PerLayer;
	PerLayer.SetNum(TileLayers.Num());
	for (int32 Index = -TileRadius; Index <= TileRadius + 1; ++Index)
	{
		int32 Kind = 0;
		if (Mod(Index, RulerPeriod) == 0)  { Kind = 2; }
		else if (Mod(Index, 4) == 0)       { Kind = 1; }

		const float Width = LineKindWidth[Kind] * Scale * LineWidthScale;
		const float Across = Index - 0.5f;
		for (float From = Start; From < End - 0.01f; From += SegmentCells)
		{
			const float To = FMath::Min(From + SegmentCells, End);
			const float Along = 0.5f * (From + To);
			const float Distance = FMath::Sqrt(Along * Along + Across * Across) / FMath::Max(TileRadius, 1);
			const float Brightness = LineKindBrightness[Kind] * (1.f - FMath::SmoothStep(FadeStart, 1.f, Distance));
			const int32 Layer = StepForBrightness(Brightness);
			if (!PerLayer.IsValidIndex(Layer))
			{
				continue;
			}
			const float Length = (To - From) * Scale;
			PerLayer[Layer].Add(FTransform(FRotator::ZeroRotator, FVector(Along, Across, 0.f) * CellSize, FVector(Length, Width, Width)));
			PerLayer[Layer].Add(FTransform(FRotator::ZeroRotator, FVector(Across, Along, 0.f) * CellSize, FVector(Width, Length, Width)));
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
	// Four beams around one cell, centred on the component (which sits on the cell's floor).
	const float Scale = CellSize / 100.f;
	const float Thickness = Width * Scale * LineWidthScale;
	const float Length = Scale + Thickness; // overlap at the corners
	const float Half = 0.5f * CellSize;
	const TArray<FTransform> Beams = {
		FTransform(FRotator::ZeroRotator, FVector(0.f, -Half, 0.f), FVector(Length, Thickness, Thickness)),
		FTransform(FRotator::ZeroRotator, FVector(0.f, Half, 0.f), FVector(Length, Thickness, Thickness)),
		FTransform(FRotator::ZeroRotator, FVector(-Half, 0.f, 0.f), FVector(Thickness, Length, Thickness)),
		FTransform(FRotator::ZeroRotator, FVector(Half, 0.f, 0.f), FVector(Thickness, Length, Thickness)),
	};
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

	// If a deployable item is selected in the inventory and the cell is free, also show a
	// small "ghost" of it.
	const FLRItemDef* Preview = nullptr;
	const int32 SlotIndex = Subsystem->GetSelectedSlot();
	if (Simulation->GetInventory().IsValidIndex(SlotIndex) && !Simulation->GetInventory()[SlotIndex].IsEmpty())
	{
		const FLRItemDef* Def = Simulation->GetData().FindItem(Simulation->GetInventory()[SlotIndex].Item);
		Preview = (Def && Def->IsPlaceable()) ? Def : nullptr;
	}

	if (Preview && !Simulation->FindPlaced(Hovered))
	{
		HoverMarker->SetVisibility(true);
		HoverMarker->SetRelativeLocation(CellToLocal(Hovered) + FVector(0.f, 0.f, 25.f * Scale));
		HoverMarker->SetRelativeScale3D(FVector(0.5f * Scale));
		if (HoverMaterial)
		{
			HoverMaterial->SetVectorParameterValue(ColorParam, Preview->GetLinearColor());
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

		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Mesh = CreateMesh(CubeMesh, Material, /*bTraceable*/ true);
		Mesh->SetRelativeLocation(Floor + FVector(0.f, 0.f, 40.f * Scale));
		Mesh->SetRelativeScale3D(FVector(0.8f * Scale));
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParam, Def ? Def->GetLinearColor() : FLinearColor::Gray);
		}

		// Irradiation enclosures: a small cube in the radiation's colour on top when a source is
		// loaded, and chamber progress in the label.
		FString LabelText = Def ? Def->Abbrev : Entity.Item.ToString();
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
