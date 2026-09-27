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
	HudWidget->AddLogMessage(TEXT("Welcome back. Scavenge for materials to get started."), false);
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

void ALRHud::HandleActionCompleted(const FLRActionResult& Result)
{
	if (HudWidget.IsValid() && !Result.Message.IsEmpty())
	{
		HudWidget->AddLogMessage(Result.Message, !Result.bSuccess);
	}
}
