#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class FLRSimulation;
class ULRGameSubsystem;

/**
 * The chamber camera's feed (docs/DESIGN.md, "Seeing the host"): a cheap stand-in until the 3D
 * scene exists, so the screen reads as a live camera rather than a black box. Painted, like the
 * dial, at a choppy FramesPerSecond (an animated GIF, more or less):
 * - the containment cage, a slowly turning wireframe sphere, reddening as the host nears it;
 * - the singularity, glowing in its Hawking colour, the glow sized by its 1 g sphere;
 * - the injector beams, dashes streaming in while feeding, and out (violet) while venting;
 * - CCTV dressing: scanlines, a blinking REC and a timestamp.
 */
class LOOTBOXRECURSION_API SLRChamberCamera : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLRChamberCamera) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/** The feed's frame rate: choppy on purpose. */
	static constexpr double FramesPerSecond = 12.0;

private:
	const FLRSimulation* GetSimulation() const;

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;
};
