#include "Game/LRHud.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/LRGameSubsystem.h"
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

	SAssignNew(HudWidget, SLRGameHud).Subsystem(Subsystem);
	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef());

	Subsystem->OnActionCompleted.AddDynamic(this, &ALRHud::HandleActionCompleted);
	Subsystem->OnWorldChanged.AddDynamic(this, &ALRHud::HandleWorldChanged);
	HudWidget->AddLogMessage(TEXT("Horizon link established. Click a cell and Perturb to seed the first ripple."), false);
}

void ALRHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULRGameSubsystem* Subsystem = ULRGameSubsystem::Get(this))
	{
		Subsystem->OnActionCompleted.RemoveDynamic(this, &ALRHud::HandleActionCompleted);
		Subsystem->OnWorldChanged.RemoveDynamic(this, &ALRHud::HandleWorldChanged);
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

void ALRHud::HandleWorldChanged()
{
	if (HudWidget.IsValid())
	{
		HudWidget->RebuildDeployedList();
	}
}

void ALRHud::HandleActionCompleted(const FLRActionResult& Result)
{
	if (HudWidget.IsValid() && !Result.Message.IsEmpty())
	{
		HudWidget->AddLogMessage(Result.Message, !Result.bSuccess);
	}
}
