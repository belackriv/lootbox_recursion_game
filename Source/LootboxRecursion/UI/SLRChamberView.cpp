#include "UI/SLRChamberView.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/LRGameSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Simulation/LRPhysics.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"

namespace
{
	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Radius,
		const FLinearColor& Color, float Thickness)
	{
		if (Radius <= 0.f)
		{
			return;
		}
		constexpr int32 Segments = 72;
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.f * UE_PI * Index / Segments;
			Points.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	void DrawLabel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text,
		const FSlateFontInfo& Font, const FVector2f& CentredAt, const FLinearColor& Color)
	{
		const FVector2f Size(FVector2D(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font)));
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(1.f, CentredAt - Size * 0.5f)),
			Text, Font, ESlateDrawEffect::None, Color);
	}

	/** Blue-white for a small, hot host; amber as it grows and cools (log of the Hawking temperature). */
	FLinearColor GlowFor(double Mass)
	{
		const double Hot = FMath::Clamp((FMath::LogX(10.0, FMath::Max(LRPhysics::HawkingTemperature(Mass), 1.0)) - 15.5) / (17.2 - 15.5), 0.0, 1.0);
		return FMath::Lerp(FLinearColor(1.f, 0.62f, 0.25f), FLinearColor(0.72f, 0.86f, 1.f), static_cast<float>(Hot));
	}
}

void SLRChamberView::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
}

const FLRSimulation* SLRChamberView::GetSimulation() const
{
	const ULRGameSubsystem* Sub = Subsystem.Get();
	return Sub ? Sub->GetSimulation() : nullptr;
}

FVector2D SLRChamberView::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(200.0, 200.0);
}

int32 SLRChamberView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FLRSimulation* Sim = GetSimulation();
	const FVector2D LocalSize = FVector2D(AllottedGeometry.GetLocalSize());
	const FVector2f Centre(static_cast<float>(LocalSize.X * 0.5), static_cast<float>(LocalSize.Y * 0.5));
	const float WallRadius = static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y) * 0.42);
	if (!Sim || !Sim->GetData().Host.IsDefined())
	{
		DrawLabel(OutDrawElements, LayerId, AllottedGeometry, TEXT("NO HOST"), Style.HeadingFont, Centre, Style.TextDim);
		return LayerId;
	}

	// Everything is to the chamber's scale; without a chamber, twice the starting host's 1 g radius.
	const FLRHostDef& Host = Sim->GetData().Host;
	const double WallMetres = Host.ChamberRadius > 0.f ? static_cast<double>(Host.ChamberRadius) : 2.0 * Host.GetGravityRadius(Host.StartMass);
	const double Mass = Sim->GetHostMass();
	const float BubbleRadius = FMath::Min(WallRadius * 1.15f, static_cast<float>(WallRadius * Host.GetGravityRadius(Mass) / FMath::Max(WallMetres, 1e-12)));
	const float Closeness = BubbleRadius / WallRadius;

	// The wall: the trip line, warming as the bubble closes on it.
	FLinearColor WallColor = Style.TextDim;
	if (Host.ChamberRadius > 0.f && Closeness >= 0.97f)
	{
		WallColor = Style.Red;
	}
	else if (Host.ChamberRadius > 0.f && Closeness >= 0.85f)
	{
		WallColor = Style.Orange;
	}
	DrawCircle(OutDrawElements, LayerId, AllottedGeometry, Centre, WallRadius, WallColor, Host.ChamberRadius > 0.f ? 3.f : 1.f);
	DrawLabel(OutDrawElements, LayerId, AllottedGeometry,
		Host.ChamberRadius > 0.f ? FString::Printf(TEXT("WALL %.1f mm"), WallMetres * 1000.0) : FString(TEXT("no chamber")),
		Style.SmallFont, Centre - FVector2f(0.f, WallRadius + 10.f), WallColor);

	if (Mass <= 0.0)
	{
		DrawLabel(OutDrawElements, LayerId + 1, AllottedGeometry, TEXT("NO HOST"), Style.HeadingFont, Centre, Style.Red);
		return LayerId + 1;
	}

	// The 1 g sphere: a glow brightening toward its shell, with a crisp edge.
	const FLinearColor Glow = GlowFor(Mass);
	constexpr int32 Rings = 12;
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float Fraction = (Ring + 0.5f) / Rings;
		const float Alpha = 0.06f + 0.3f * Fraction * Fraction;
		DrawCircle(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, BubbleRadius * Fraction,
			FLinearColor(Glow.R, Glow.G, Glow.B, Alpha), BubbleRadius / Rings + 0.75f);
	}
	DrawCircle(OutDrawElements, LayerId + 2, AllottedGeometry, Centre, BubbleRadius, FLinearColor(Glow.R, Glow.G, Glow.B, 0.95f), 2.f);
	DrawLabel(OutDrawElements, LayerId + 2, AllottedGeometry, TEXT("1 g"), Style.SmallFont,
		Centre + FVector2f(0.f, FMath::Max(BubbleRadius - 10.f, 8.f)), FLinearColor(Glow.R, Glow.G, Glow.B, 1.f));

	// The host itself: far too small to see, so a bright point.
	DrawCircle(OutDrawElements, LayerId + 3, AllottedGeometry, Centre, 1.5f, FLinearColor::White, 3.f);
	return LayerId + 3;
}
