#include "Cosmos/LRCosmosActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LootboxRecursion.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Parameter names of the engine's Widget3DPassThrough materials (see UWidgetComponent).
	const FName TextureParam(TEXT("SlateUI"));
	const FName TintParam(TEXT("TintColorAndOpacity"));
	const FName OpacityFromTextureParam(TEXT("OpacityFromTexture"));
	const FName BackColorParam(TEXT("BackColor"));

	UTexture2D* MakeSolidTexture(const FColor& Color)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, &Color, sizeof(FColor));
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

ALRCosmosActor::ALRCosmosActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Follow the camera after it has moved this frame, so the sky never lags behind.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueFinder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentFinder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent"));
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

	WhiteTexture = MakeSolidTexture(FColor::White);

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
		ExcludeFromLighting(Backdrop);
		Backdrop->SetRelativeScale3D(FVector(SkyRadius / 50.f)); // engine sphere radius is 50
		Backdrop->RegisterComponent();
		Backdrop->SetMaterial(0, MakeUnlitMaterial(UnlitOpaqueMaterial, WhiteTexture, FLinearColor(0.002f, 0.002f, 0.004f, 1.f)));
	}

	BuildStars();
	BuildBlackHole();
}

void ALRCosmosActor::ExcludeFromLighting(UPrimitiveComponent* Component)
{
	// The sky is scenery at infinity: keep it out of Lumen, distance fields and ray tracing,
	// which would otherwise see a solid sphere enclosing the whole scene.
	Component->bAffectDistanceFieldLighting = false;
	Component->bAffectDynamicIndirectLighting = false;
	Component->bVisibleInRayTracing = false;
}

UMaterialInstanceDynamic* ALRCosmosActor::MakeUnlitMaterial(UMaterialInterface* Parent, UTexture2D* Texture, const FLinearColor& Tint)
{
	UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (Material)
	{
		Material->SetTextureParameterValue(TextureParam, Texture);
		Material->SetVectorParameterValue(TintParam, Tint);
		Material->SetVectorParameterValue(BackColorParam, FLinearColor::Black);
		Material->SetScalarParameterValue(OpacityFromTextureParam, 1.f);
	}
	return Material;
}

UInstancedStaticMeshComponent* ALRCosmosActor::MakeStarLayer(const FLinearColor& Tint)
{
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this);
	Layer->SetStaticMesh(CubeMesh);
	Layer->SetupAttachment(RootComponent);
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Layer->SetCastShadow(false);
	ExcludeFromLighting(Layer);
	Layer->RegisterComponent();
	if (UMaterialInstanceDynamic* Material = MakeUnlitMaterial(UnlitOpaqueMaterial, WhiteTexture, Tint))
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
	ExcludeFromLighting(BlackHole);
	BlackHole->SetRelativeLocation(Direction * Distance);
	BlackHole->SetRelativeRotation(Rotation);
	BlackHole->SetRelativeScale3D(FVector(Width / 100.f, Width / 100.f, 1.f)); // engine plane is 100cm
	BlackHole->RegisterComponent();
	BlackHole->SetMaterial(0, MakeUnlitMaterial(UnlitTranslucentMaterial, BlackHoleTexture, FLinearColor(Brightness, Brightness, Brightness, 1.f)));

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

void ALRCosmosActor::HideLevelEnvironment()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
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
		}
		// The Basic level template's ground plane.
		if (Actor->IsA<AStaticMeshActor>() && Actor->GetName().StartsWith(TEXT("Floor")))
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
}
