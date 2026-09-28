#include "Cosmos/LRCosmosActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRWorldGridActor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "LootboxRecursion.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Rendering/LRMaterialHooks.h"
#include "Rendering/LRUnlit.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

ALRCosmosActor::ALRCosmosActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Follow the camera after it has moved this frame, so the sky never lags behind.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueFinder(LRUnlit::OpaqueMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentFinder(LRUnlit::TranslucentMaterialPath);
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	PlaneMesh = PlaneFinder.Object;
	UnlitOpaqueMaterial = OpaqueFinder.Object;
	UnlitTranslucentMaterial = TranslucentFinder.Object;
}

void ALRCosmosActor::BeginPlay()
{
	Super::BeginPlay();

	if (bHideLevelEnvironment)
	{
		HideLevelEnvironment();
	}

	WhiteTexture = LRUnlit::MakeSolidTexture(FColor::White);
	BackdropHook = LRMaterialHooks::LoadOptional(BackdropMaterialPath);
	StarHook = LRMaterialHooks::LoadOptional(StarMaterialPath);
	BlackHoleHook = LRMaterialHooks::LoadOptional(BlackHoleMaterialPath);
	PlasmaHook = LRMaterialHooks::LoadOptional(PlasmaMaterialPath);

	// Fixed exposure: auto exposure would brighten the black void until it turns grey, blow out
	// the disk, and pump as the grid or UI fill the view.
	UPostProcessComponent* PostProcess = NewObject<UPostProcessComponent>(this);
	PostProcess->bUnbound = true;
	PostProcess->Settings.bOverride_AutoExposureMethod = true;
	PostProcess->Settings.AutoExposureMethod = AEM_Manual;
	PostProcess->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PostProcess->Settings.AutoExposureApplyPhysicalCameraExposure = false;
	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = ExposureBias;
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->RegisterComponent();

	// The void: a huge two-sided black sphere that hides whatever sky the level has.
	if (SphereMesh && UnlitOpaqueMaterial)
	{
		Backdrop = NewObject<UStaticMeshComponent>(this);
		Backdrop->SetStaticMesh(SphereMesh);
		Backdrop->SetupAttachment(RootComponent);
		Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Backdrop->SetCastShadow(false);
		LRUnlit::ExcludeFromLighting(Backdrop);
		Backdrop->SetRelativeScale3D(FVector(SkyRadius / 50.f)); // engine sphere radius is 50
		Backdrop->RegisterComponent();
		Backdrop->SetMaterial(0, LRMaterialHooks::MakeOr(BackdropHook, UnlitOpaqueMaterial, this, WhiteTexture, FLinearColor(0.002f, 0.002f, 0.004f, 1.f)));
	}

	BuildStars();
	BuildBlackHole();
	BuildPlasmaVeil();
}

void ALRCosmosActor::BuildPlasmaVeil()
{
	if (!SphereMesh || !UnlitTranslucentMaterial)
	{
		return;
	}
	// Half the sky's radius: in front of the stars and the black hole, far behind the grid.
	// The engine materials used here are two-sided, so it shows from the inside.
	PlasmaVeil = NewObject<UStaticMeshComponent>(this);
	PlasmaVeil->SetStaticMesh(SphereMesh);
	PlasmaVeil->SetupAttachment(RootComponent);
	PlasmaVeil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlasmaVeil->SetCastShadow(false);
	LRUnlit::ExcludeFromLighting(PlasmaVeil);
	PlasmaVeil->SetRelativeScale3D(FVector(0.5f * SkyRadius / 50.f)); // engine sphere radius is 50
	// Draw after the black hole plane, which is also translucent.
	PlasmaVeil->TranslucencySortPriority = 10;
	PlasmaVeil->SetVisibility(false);
	PlasmaVeil->RegisterComponent();
	PlasmaMaterial = LRMaterialHooks::MakeOr(PlasmaHook, UnlitTranslucentMaterial, this, WhiteTexture, FLinearColor(PlasmaColor.R, PlasmaColor.G, PlasmaColor.B, 0.f));
	PlasmaVeil->SetMaterial(0, PlasmaMaterial);
	UpdatePlasmaVeil();
}

void ALRCosmosActor::UpdatePlasmaVeil()
{
	if (!PlasmaVeil)
	{
		return;
	}
	const ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	const float Opacity = Subsystem ? FMath::Clamp(Subsystem->GetPlasmaOpacity(), 0.f, 1.f) * MaxPlasmaOpacity : 0.f;
	if (FMath::IsNearlyEqual(Opacity, ShownPlasmaOpacity, 0.002f))
	{
		return;
	}
	ShownPlasmaOpacity = Opacity;
	PlasmaVeil->SetVisibility(Opacity > 0.002f);
	LRMaterialHooks::SetColor(PlasmaMaterial, FLinearColor(PlasmaColor.R, PlasmaColor.G, PlasmaColor.B, Opacity));
}

UInstancedStaticMeshComponent* ALRCosmosActor::MakeStarLayer(const FLinearColor& Tint)
{
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this);
	Layer->SetStaticMesh(CubeMesh);
	Layer->SetupAttachment(RootComponent);
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Layer->SetCastShadow(false);
	LRUnlit::ExcludeFromLighting(Layer);
	Layer->RegisterComponent();
	if (UMaterialInstanceDynamic* Material = LRMaterialHooks::MakeOr(StarHook, UnlitOpaqueMaterial, this, WhiteTexture, Tint))
	{
		Layer->SetMaterial(0, Material);
	}
	StarLayers.Add(Layer);
	return Layer;
}

void ALRCosmosActor::BuildStars()
{
	if (!CubeMesh || !UnlitOpaqueMaterial)
	{
		return;
	}

	// Three tints (one draw call each): blue-white, warm, and a dim dusty band.
	const float B = Brightness;
	UInstancedStaticMeshComponent* StarLayerComps[] = {
		MakeStarLayer(FLinearColor(0.75f * B, 0.85f * B, 1.0f * B, 1.f)),
		MakeStarLayer(FLinearColor(1.0f * B, 0.8f * B, 0.6f * B, 1.f)),
		MakeStarLayer(FLinearColor(0.25f, 0.28f, 0.4f, 1.f)),
	};

	// A tilted "galactic plane": the dim layer crowds toward it.
	const FVector BandNormal = FVector(0.3f, -0.4f, 1.f).GetSafeNormal();
	const float StarDistance = SkyRadius * 0.95f;
	FRandomStream Random(20260927);
	TArray<FTransform> PerLayer[3];

	for (int32 Index = 0; Index < StarCount; ++Index)
	{
		const int32 Layer = Index % 5 == 0 ? 1 : (Index % 5 < 3 ? 0 : 2);
		FVector Direction = Random.GetUnitVector();
		if (Layer == 2)
		{
			// Squash toward the band plane.
			Direction -= BandNormal * FVector::DotProduct(Direction, BandNormal) * 0.85f;
			Direction.Normalize();
		}
		// Angular size ~0.1 - 0.25 degrees (a few pixels at 1080p), the dusty band smaller.
		const float Size = StarDistance * FMath::DegreesToRadians(Layer == 2 ? Random.FRandRange(0.08f, 0.15f) : Random.FRandRange(0.1f, 0.25f)) / 100.f;
		PerLayer[Layer].Add(FTransform(FRotator(Random.FRandRange(0.f, 90.f), Random.FRandRange(0.f, 90.f), 0.f), Direction * StarDistance, FVector(Size)));
	}
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		if (StarLayerComps[Layer])
		{
			StarLayerComps[Layer]->AddInstances(PerLayer[Layer], /*bShouldReturnIndices*/ false);
		}
	}
}

void ALRCosmosActor::BuildBlackHole()
{
	FString Error;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Cosmos/BlackHole.lrbh");
	if (!Renderer.LoadFromFile(Path, Error))
	{
		UE_LOG(LogLootbox, Warning, TEXT("Cosmos: no black hole (%s). Run Tools/cosmos/generate_black_hole.py."), *Error);
		return;
	}
	Renderer.SpinSeconds = DiskSpinSeconds;
	if (!PlaneMesh || !UnlitTranslucentMaterial)
	{
		return;
	}

	BlackHoleTexture = UTexture2D::CreateTransient(Renderer.GetWidth(), Renderer.GetHeight(), PF_B8G8R8A8);
	if (!BlackHoleTexture)
	{
		return;
	}
	BlackHoleTexture->SRGB = true;
	BlackHoleTexture->Filter = TF_Bilinear;
	BlackHoleTexture->AddressX = TA_Clamp;
	BlackHoleTexture->AddressY = TA_Clamp;
	BlackHoleTexture->UpdateResource();

	// A camera-facing square far away in BlackHoleDirection, sized to the requested angle.
	const FVector Direction = BlackHoleDirection.GetSafeNormal();
	const float Distance = SkyRadius * 0.8f;
	// Past ~55 degrees the quad's corners would poke through the sky sphere.
	const float Width = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(BlackHoleAngularSize, 5.f, 55.f) * 0.5f));
	FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}
	// Plane normal (+Z) faces the camera; its +X is screen-right, so the image is upright.
	FRotator Rotation = FRotationMatrix::MakeFromZX(-Direction, Right).Rotator();
	Rotation = (FQuat(-Direction, FMath::DegreesToRadians(BlackHoleRoll)) * Rotation.Quaternion()).Rotator();

	BlackHole = NewObject<UStaticMeshComponent>(this);
	BlackHole->SetStaticMesh(PlaneMesh);
	BlackHole->SetupAttachment(RootComponent);
	BlackHole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlackHole->SetCastShadow(false);
	LRUnlit::ExcludeFromLighting(BlackHole);
	BlackHole->SetRelativeLocation(Direction * Distance);
	BlackHole->SetRelativeRotation(Rotation);
	BlackHole->SetRelativeScale3D(FVector(Width / 100.f, Width / 100.f, 1.f)); // engine plane is 100cm
	BlackHole->RegisterComponent();
	BlackHole->SetMaterial(0, LRMaterialHooks::MakeOr(BlackHoleHook, UnlitTranslucentMaterial, this, BlackHoleTexture, FLinearColor(Brightness, Brightness, Brightness, 1.f)));

	UploadFrame();
}

void ALRCosmosActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Stay centred on the camera: the sky is infinitely far away.
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		SetActorLocation(CameraManager->GetCameraLocation());
	}

	UpdatePlasmaVeil();

	AnimationTime += DeltaSeconds;
	SinceLastFrame += DeltaSeconds;
	if (BlackHoleTexture && SinceLastFrame >= 1.0 / FMath::Max(FramesPerSecond, 1.f))
	{
		SinceLastFrame = 0.0;
		UploadFrame();
	}
}

void ALRCosmosActor::UploadFrame()
{
	// No render resource (e.g. -nullrhi): the cleanup callback would never run, so don't allocate.
	if (!BlackHoleTexture || !BlackHoleTexture->GetResource())
	{
		UE_LOG(LogLootbox, Verbose, TEXT("Cosmos: black hole texture has no render resource, frame skipped"));
		return;
	}
	Renderer.Render(AnimationTime, FramePixels);

	// UpdateTextureRegions copies on the render thread later, so hand it its own buffer.
	const int32 Pitch = Renderer.GetWidth() * static_cast<int32>(sizeof(FColor));
	const int32 Bytes = Pitch * Renderer.GetHeight();
	uint8* Copy = new uint8[Bytes];
	FMemory::Memcpy(Copy, FramePixels.GetData(), Bytes);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Renderer.GetWidth(), Renderer.GetHeight());
	BlackHoleTexture->UpdateTextureRegions(0, 1, Region, Pitch, sizeof(FColor), Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

void ALRCosmosActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Super::EndPlay(EndPlayReason);
}

void ALRCosmosActor::HideLevelEnvironment()
{
	for (ULevel* Level : GetWorld()->GetLevels())
	{
		HideLevelContent(Level);
	}
	// World Partition and level streaming load terrain in pieces after BeginPlay.
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ALRCosmosActor::HandleLevelAdded);
}

void ALRCosmosActor::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld())
	{
		HideLevelContent(Level);
	}
}

void ALRCosmosActor::HideLevelContent(ULevel* Level)
{
	if (!Level)
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		// Keep the game's own actors. Lights are not primitives, so they keep lighting the entities.
		if (!IsValid(Actor) || Actor == this || Actor->IsA<APawn>() || Actor->IsA<ALRWorldGridActor>())
		{
			continue;
		}
		TArray<USceneComponent*> Components;
		Actor->GetComponents<USceneComponent>(Components);
		for (USceneComponent* Component : Components)
		{
			if (Component->IsA<USkyAtmosphereComponent>() || Component->IsA<UVolumetricCloudComponent>()
				|| Component->IsA<UExponentialHeightFogComponent>())
			{
				Component->SetVisibility(false);
			}
			// A real-time capture sky light needs a sky to capture, and we just hid it: the engine
			// then prints a red warning every frame and the light turns black anyway.
			else if (const USkyLightComponent* SkyLight = Cast<USkyLightComponent>(Component))
			{
				if (SkyLight->bRealTimeCapture)
				{
					Component->SetVisibility(false);
				}
			}
			// Floors, landscapes, template props: the void has no ground.
			else if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetHiddenInGame(true);
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}
