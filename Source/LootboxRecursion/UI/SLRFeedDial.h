#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class FLRSimulation;
class ULRGameSubsystem;

/**
 * The feed dial on the outside panel (docs/DESIGN.md, "Feeding the host"): a logarithmic gauge
 * of the injectors' rate, kg/s. Drag it or scroll it to set the target; the flow needle follows
 * with inertia.
 *
 * - The arc runs from OFF (bottom left) through MinRate to the injectors' maximum (bottom right).
 * - The rated limit mark (LIMIT) and the break-even mark (HOLD, the evaporation rate) move with
 *   the host's mass. Between them the host grows (green); above the limit the flow is wasted
 *   (red).
 * - The thin needle is the dial setting, and the thick one the actual flow.
 *
 * Painted directly (a leaf widget), so it needs no textures.
 */
class LOOTBOXRECURSION_API SLRFeedDial : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLRFeedDial) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** The lowest marked rate, kg/s. Below it the dial runs linearly down to OFF. */
	static constexpr double MinRate = 10.0;

	/** Position on the arc (0 = OFF, 1 = full) for a rate, logarithmic above MinRate. */
	static double RateToFraction(double Rate, double MaxRate);
	/** The inverse of RateToFraction. */
	static double FractionToRate(double Fraction, double MaxRate);

private:
	const FLRSimulation* GetSimulation() const;
	double GetMaxRate() const;
	/** Set the dial from a point in the widget (local coordinates). */
	void SetTargetFromPoint(const FGeometry& MyGeometry, const FVector2D& ScreenPosition);

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;
	bool bDragging = false;
};
