#include "Game/LRGameMode.h"

#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/LRCameraPawn.h"
#include "Game/LRHud.h"
#include "Game/LRPlayerController.h"
#include "Game/LRWorldLineActor.h"

ALRGameMode::ALRGameMode()
{
	DefaultPawnClass = ALRCameraPawn::StaticClass();
	PlayerControllerClass = ALRPlayerController::StaticClass();
	HUDClass = ALRHud::StaticClass();
}

void ALRGameMode::StartPlay()
{
	EnsureWorldView();
	EnsureLighting();
	Super::StartPlay();
}

void ALRGameMode::EnsureWorldView()
{
	// If a designer placed one in the level, use it; otherwise spawn a default.
	if (TActorIterator<ALRWorldLineActor>(GetWorld()))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<ALRWorldLineActor>(FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, Params);
}

void ALRGameMode::EnsureLighting()
{
	// A level made from the "Basic" template already has a sun, sky and fog.
	if (TActorIterator<ADirectionalLight>(GetWorld()))
	{
		return;
	}

	struct FLightSpec
	{
		FRotator Rotation;
		float Intensity;
		bool bShadows;
	};
	const FLightSpec Lights[] = {
		{ FRotator(-50.f, 30.f, 0.f), 6.f, true },   // key
		{ FRotator(-20.f, 200.f, 0.f), 1.5f, false }, // fill from the other side
	};

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FLightSpec& Spec : Lights)
	{
		if (ADirectionalLight* Light = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, Spec.Rotation, Params))
		{
			if (ULightComponent* Component = Light->GetLightComponent())
			{
				Component->SetMobility(EComponentMobility::Movable);
				Component->SetIntensity(Spec.Intensity);
				Component->SetCastShadows(Spec.bShadows);
			}
		}
	}
}
