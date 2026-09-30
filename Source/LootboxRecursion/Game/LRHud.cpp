#include "Game/LRHud.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/LRGameSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/LRMaterialHooks.h"
#include "UI/SLRGameHud.h"

void ALRHud::BeginPlay()
{
	Super::BeginPlay();

	ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this);
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Subsystem || !Viewport)
	{
		return;
	}

	StaticHook = LRMaterialHooks::LoadOptional(StaticMaterialPath);
	SAssignNew(HudWidget, SLRGameHud).Subsystem(Subsystem).StaticMaterial(StaticHook.Get());
	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef());

	Subsystem->OnActionCompleted.AddDynamic(this, &ALRHud::HandleActionCompleted);
	HudWidget->AddLogMessage(TEXT("Horizon link established. Click a cell and Perturb to seed the first ripple."), false);
}

void ALRHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnActionCompleted.RemoveDynamic(this, &ALRHud::HandleActionCompleted);
	}
	if (HudWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
		}
		HudWidget.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

bool ALRHud::IsMenuOpen() const
{
	return HudWidget.IsValid() && HudWidget->IsMenuOpen();
}

void ALRHud::HandleMenuKey()
{
	if (HudWidget.IsValid())
	{
		HudWidget->HandleMenuKey();
	}
}

void ALRHud::ActivateCardSlot(int32 Slot)
{
	if (HudWidget.IsValid())
	{
		HudWidget->ActivateCardSlot(Slot);
	}
}

void ALRHud::NudgeDial(float Direction, bool bCoarse, float DeltaSeconds)
{
	if (HudWidget.IsValid())
	{
		HudWidget->NudgeDial(Direction, bCoarse, DeltaSeconds);
	}
}

void ALRHud::HandleActionCompleted(const FLRActionResult& Result)
{
	if (HudWidget.IsValid() && !Result.Message.IsEmpty())
	{
		HudWidget->AddLogMessage(Result.Message, !Result.bSuccess);
	}
}
