#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class FLRSimulation;
class ULRGameSubsystem;

/**
 * The host, seen from outside (docs/DESIGN.md, "Seeing the host"): a to-scale cross-section of
 * the containment chamber. The wall is the trip line, and the glowing bubble is the host's
 * 1 g sphere, which swells with the square root of its mass. The bubble's colour follows the
 * Hawking temperature: blue-white when the host is small, hot and in danger, warmer as it grows.
 */
class LOOTBOXRECURSION_API SLRChamberView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLRChamberView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	const FLRSimulation* GetSimulation() const;

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;
};
