#include "UI/SLRChamberView.h"

#include "Game/LRGameSubsystem.h"
#include "Simulation/LRPhysics.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "UI/LRSlateDraw.h"

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
		LRSlateDraw::Label(OutDrawElements, LayerId, AllottedGeometry, TEXT("NO HOST"), Style.HeadingFont, Centre, Style.TextDim);
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
	LRSlateDraw::Circle(OutDrawElements, LayerId, AllottedGeometry, Centre, WallRadius, WallColor, Host.ChamberRadius > 0.f ? 3.f : 1.f);
	LRSlateDraw::Label(OutDrawElements, LayerId, AllottedGeometry,
		Host.ChamberRadius > 0.f ? FString::Printf(TEXT("WALL %.1f mm"), WallMetres * 1000.0) : FString(TEXT("no chamber")),
		Style.SmallFont, Centre - FVector2f(0.f, WallRadius + 10.f), WallColor);

	if (Mass <= 0.0)
	{
		LRSlateDraw::Label(OutDrawElements, LayerId + 1, AllottedGeometry, TEXT("NO HOST"), Style.HeadingFont, Centre, Style.Red);
		return LayerId + 1;
	}

	// The point of no return, as the 1 g radius it would have: a bubble inside this ring can't be
	// fed out of evaporating.
	const double Tipping = Sim->GetTippingMass();
	if (Tipping > 0.0 && Tipping < 1e30)
	{
		const float TippingRadius = static_cast<float>(WallRadius * Host.GetGravityRadius(Tipping) / FMath::Max(WallMetres, 1e-12));
		const bool bBelow = Mass < Tipping;
		const FLinearColor NoReturn = bBelow ? Style.Red : FLinearColor(Style.Red.R, Style.Red.G, Style.Red.B, 0.45f);
		LRSlateDraw::Circle(OutDrawElements, LayerId, AllottedGeometry, Centre, TippingRadius, NoReturn, 1.f);
		LRSlateDraw::Label(OutDrawElements, LayerId, AllottedGeometry, TEXT("no return"), Style.SmallFont,
			Centre - FVector2f(0.f, TippingRadius + 8.f), NoReturn);
	}

	// The 1 g sphere: a glow brightening toward its shell, with a crisp edge.
	const FLinearColor BubbleColor = LRSlateDraw::HawkingGlow(Mass);
	constexpr int32 Rings = 12;
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float Fraction = (Ring + 0.5f) / Rings;
		const float Alpha = 0.06f + 0.3f * Fraction * Fraction;
		LRSlateDraw::Circle(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, BubbleRadius * Fraction,
			FLinearColor(BubbleColor.R, BubbleColor.G, BubbleColor.B, Alpha), BubbleRadius / Rings + 0.75f);
	}
	LRSlateDraw::Circle(OutDrawElements, LayerId + 2, AllottedGeometry, Centre, BubbleRadius, FLinearColor(BubbleColor.R, BubbleColor.G, BubbleColor.B, 0.95f), 2.f);
	LRSlateDraw::Label(OutDrawElements, LayerId + 2, AllottedGeometry, TEXT("1 g"), Style.SmallFont,
		Centre + FVector2f(0.f, FMath::Max(BubbleRadius - 10.f, 8.f)), FLinearColor(BubbleColor.R, BubbleColor.G, BubbleColor.B, 1.f));

	// The host itself: far too small to see, so a bright point.
	LRSlateDraw::Circle(OutDrawElements, LayerId + 3, AllottedGeometry, Centre, 1.5f, FLinearColor::White, 3.f);
	return LayerId + 3;
}
