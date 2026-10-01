#include "UI/SLRFeedDial.h"

#include "Game/LRGameSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "UI/LRSlateDraw.h"

namespace
{
	// The arc: from bottom left (OFF), over the top, to bottom right (full). Screen Y points down,
	// so angles grow clockwise.
	constexpr double StartDegrees = 135.0;
	constexpr double SweepDegrees = 270.0;
	/** The first stretch of the arc, from OFF to MinRate, runs linearly. */
	constexpr double OffBand = 0.06;

	FVector2f PointAt(const FVector2f& Centre, float Radius, double Fraction)
	{
		const double Radians = FMath::DegreesToRadians(StartDegrees + SweepDegrees * Fraction);
		return Centre + FVector2f(static_cast<float>(FMath::Cos(Radians)), static_cast<float>(FMath::Sin(Radians))) * Radius;
	}

	void DrawArc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Radius,
		double From, double To, const FLinearColor& Color, float Thickness)
	{
		if (To <= From)
		{
			return;
		}
		const int32 Segments = FMath::Max(2, FMath::CeilToInt32((To - From) * 72.0));
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			Points.Add(PointAt(Centre, Radius, From + (To - From) * Index / Segments));
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** A line along the radius at Fraction, from Inner to Outer. */
	void DrawRadial(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
		double Fraction, float Inner, float Outer, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Add(PointAt(Centre, Inner, Fraction));
		Points.Add(PointAt(Centre, Outer, Fraction));
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

}

void SLRFeedDial::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
}

const FLRSimulation* SLRFeedDial::GetSimulation() const
{
	const ULRGameSubsystem* Sub = Subsystem.Get();
	return Sub ? Sub->GetSimulation() : nullptr;
}

double SLRFeedDial::GetMaxRate() const
{
	const FLRSimulation* Sim = GetSimulation();
	return Sim ? FMath::Max(Sim->GetData().Host.InjectorMaxRate, MinRate * 10.0) : MinRate * 1000.0;
}

double SLRFeedDial::RateToFraction(double Rate, double MaxRate)
{
	if (Rate <= 0.0)
	{
		return 0.0;
	}
	if (Rate < MinRate)
	{
		return OffBand * Rate / MinRate;
	}
	const double Decades = FMath::Loge(FMath::Max(MaxRate, MinRate * 1.01) / MinRate);
	return FMath::Clamp(OffBand + (1.0 - OffBand) * FMath::Loge(Rate / MinRate) / Decades, 0.0, 1.0);
}

double SLRFeedDial::FractionToRate(double Fraction, double MaxRate)
{
	if (Fraction <= 0.01)
	{
		return 0.0; // snap to OFF
	}
	if (Fraction < OffBand)
	{
		return MinRate * Fraction / OffBand;
	}
	const double Decades = FMath::Loge(FMath::Max(MaxRate, MinRate * 1.01) / MinRate);
	return MinRate * FMath::Exp((FMath::Min(Fraction, 1.0) - OffBand) / (1.0 - OffBand) * Decades);
}

FVector2D SLRFeedDial::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(250.0, 230.0);
}

void SLRFeedDial::SetTargetFromPoint(const FGeometry& MyGeometry, const FVector2D& ScreenPosition)
{
	ULRGameSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return;
	}
	const FVector2D Local = FVector2D(MyGeometry.AbsoluteToLocal(ScreenPosition));
	const FVector2D Size = FVector2D(MyGeometry.GetLocalSize());
	const FVector2D Centre(Size.X * 0.5, Size.Y * 0.52);
	const FVector2D Offset = Local - Centre;
	double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
	while (Degrees < StartDegrees)
	{
		Degrees += 360.0;
	}
	double Fraction = (Degrees - StartDegrees) / SweepDegrees;
	if (Fraction > 1.0)
	{
		// The gap at the bottom: snap to whichever end is nearer.
		Fraction = Degrees < StartDegrees + SweepDegrees + 45.0 ? 1.0 : 0.0;
	}
	Sub->SetInjectorTarget(FractionToRate(Fraction, GetMaxRate()));
}

FReply SLRFeedDial::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	SetTargetFromPoint(MyGeometry, FVector2D(MouseEvent.GetScreenSpacePosition()));
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SLRFeedDial::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SLRFeedDial::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	SetTargetFromPoint(MyGeometry, FVector2D(MouseEvent.GetScreenSpacePosition()));
	return FReply::Handled();
}

FReply SLRFeedDial::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ULRGameSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return FReply::Unhandled();
	}
	// One notch is 2% of the arc (about a fifteenth of a decade at the shipped range).
	const double MaxRate = GetMaxRate();
	const double Fraction = RateToFraction(Sub->GetInjectorTarget(), MaxRate) + 0.02 * MouseEvent.GetWheelDelta();
	Sub->SetInjectorTarget(FractionToRate(FMath::Clamp(Fraction, 0.0, 1.0), MaxRate));
	return FReply::Handled();
}

int32 SLRFeedDial::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FLRSimulation* Sim = GetSimulation();
	const FVector2D LocalSize = FVector2D(AllottedGeometry.GetLocalSize());
	const FVector2f Centre(static_cast<float>(LocalSize.X * 0.5), static_cast<float>(LocalSize.Y * 0.52));
	const float Radius = static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y) * 0.36);
	const double MaxRate = GetMaxRate();

	const FLinearColor Track(0.13f, 0.15f, 0.21f, 1.f);
	const FLinearColor GrowZone(Style.Green.R, Style.Green.G, Style.Green.B, 0.55f);
	const FLinearColor WasteZone(Style.Red.R, Style.Red.G, Style.Red.B, 0.45f);
	const FLinearColor HoldColor(0.45f, 0.85f, 1.f, 1.f);

	// The track, then the zones: grows between HOLD and LIMIT, wasted above LIMIT.
	DrawArc(OutDrawElements, LayerId, AllottedGeometry, Centre, Radius, 0.0, 1.0, Track, 8.f);
	const double Limit = Sim ? RateToFraction(Sim->GetRatedLimit(), MaxRate) : 1.0;
	const double Hold = Sim ? RateToFraction(Sim->GetEvaporationRate(), MaxRate) : 0.0;
	const bool bHostAlive = Sim && !Sim->IsFrozen();
	// Reversed (venting, or winding down after it): the dial sets how hard the injectors pull.
	// Everything up to LIMIT is drawn out, and HOLD means nothing.
	const bool bReversed = Sim && (Sim->IsVenting() || Sim->GetInjectorFlow() < 0.0);
	const FLinearColor VentZone(Style.Vent.R, Style.Vent.G, Style.Vent.B, 0.45f);
	if (bHostAlive && bReversed)
	{
		DrawArc(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, Radius, 0.0, Limit, VentZone, 8.f);
		LRSlateDraw::Label(OutDrawElements, LayerId + 1, AllottedGeometry, TEXT("REVERSED"), Style.SmallFont,
			Centre - FVector2f(0.f, Radius * 0.45f), Style.Vent);
	}
	else if (bHostAlive)
	{
		DrawArc(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, Radius, Hold, Limit, GrowZone, 8.f);
		DrawArc(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, Radius, Limit, 1.0, WasteZone, 8.f);
	}

	// Decade ticks and labels.
	const TPair<double, const TCHAR*> Ticks[] = {
		{ 10.0, TEXT("10 kg/s") }, { 100.0, TEXT("100") }, { 1000.0, TEXT("1 t/s") }, { 10000.0, TEXT("10 t/s") }, { 100000.0, TEXT("100 t/s") },
	};
	DrawRadial(OutDrawElements, LayerId + 2, AllottedGeometry, Centre, 0.0, Radius - 6.f, Radius + 6.f, Style.TextDim, 1.5f);
	LRSlateDraw::Label(OutDrawElements, LayerId + 2, AllottedGeometry, TEXT("OFF"), Style.SmallFont, PointAt(Centre, Radius + 20.f, 0.0), Style.TextDim);
	for (const TPair<double, const TCHAR*>& Tick : Ticks)
	{
		if (Tick.Key > MaxRate * 1.001)
		{
			continue;
		}
		const double Fraction = RateToFraction(Tick.Key, MaxRate);
		DrawRadial(OutDrawElements, LayerId + 2, AllottedGeometry, Centre, Fraction, Radius - 6.f, Radius + 6.f, Style.TextDim, 1.5f);
		LRSlateDraw::Label(OutDrawElements, LayerId + 2, AllottedGeometry, Tick.Value, Style.SmallFont, PointAt(Centre, Radius + 22.f, Fraction), Style.TextDim);
	}

	// The moving marks: the rated limit and the break-even (evaporation) rate.
	if (bHostAlive)
	{
		DrawRadial(OutDrawElements, LayerId + 3, AllottedGeometry, Centre, Limit, Radius - 14.f, Radius + 10.f, Style.Orange, 3.f);
		LRSlateDraw::Label(OutDrawElements, LayerId + 3, AllottedGeometry, TEXT("LIMIT"), Style.SmallFont, PointAt(Centre, Radius - 26.f, Limit), Style.Orange);
		if (!bReversed)
		{
			DrawRadial(OutDrawElements, LayerId + 3, AllottedGeometry, Centre, Hold, Radius - 14.f, Radius + 10.f, HoldColor, 3.f);
			LRSlateDraw::Label(OutDrawElements, LayerId + 3, AllottedGeometry, TEXT("HOLD"), Style.SmallFont, PointAt(Centre, Radius - 26.f, Hold), HoldColor);
		}
	}

	// Needles: the dial setting (thin) and the actual flow (thick: green in, red above the limit,
	// violet when it runs in reverse).
	const double Target = Sim ? Sim->GetInjectorTarget() : 0.0;
	const double Flow = Sim ? Sim->GetInjectorFlow() : 0.0;
	const bool bWasting = Sim && FMath::Abs(Flow) > Sim->GetRatedLimit() * 1.001;
	const FLinearColor FlowColor = Flow < 0.0 ? Style.Vent : (bWasting ? Style.Red : Style.Green);
	DrawRadial(OutDrawElements, LayerId + 4, AllottedGeometry, Centre, RateToFraction(Target, MaxRate), 0.f, Radius - 2.f, Style.Text, 1.5f);
	DrawRadial(OutDrawElements, LayerId + 5, AllottedGeometry, Centre, RateToFraction(FMath::Abs(Flow), MaxRate), 0.f, Radius - 16.f,
		FlowColor, 4.f);

	// The readout in the middle.
	FString Status = Flow < 0.0 ? FString::Printf(TEXT("%s out"), *FLRSimulation::FormatRate(-Flow)) : FLRSimulation::FormatRate(Flow);
	FLinearColor StatusColor = Flow < 0.0 ? Style.Vent : Style.Text;
	if (!Sim || !Sim->GetData().Host.IsDefined())
	{
		Status = TEXT("NO HOST");
		StatusColor = Style.TextDim;
	}
	else if (Sim->IsFrozen())
	{
		Status = TEXT("NO HOST");
		StatusColor = Style.Red;
	}
	LRSlateDraw::Label(OutDrawElements, LayerId + 6, AllottedGeometry, Status, Style.HeadingFont, Centre + FVector2f(0.f, Radius * 0.45f), StatusColor);
	const ELRInjectorAuto Auto = Sim ? Sim->GetInjectorAuto() : ELRInjectorAuto::Off;
	const TCHAR* AutoMode = (Sim && Sim->IsVenting()) ? TEXT("vent ")
		: (Auto == ELRInjectorAuto::Hold ? TEXT("auto hold ") : (Auto == ELRInjectorAuto::Limit ? TEXT("auto limit ") : TEXT("dial ")));
	LRSlateDraw::Label(OutDrawElements, LayerId + 6, AllottedGeometry, FString::Printf(TEXT("%s%s"), AutoMode, *FLRSimulation::FormatRate(Target)),
		Style.SmallFont, Centre + FVector2f(0.f, Radius * 0.45f + 16.f), Style.TextDim);
	return LayerId + 6;
}
