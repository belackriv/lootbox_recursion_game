#include "UI/SLRChamberCamera.h"

#include "Game/LRGameSubsystem.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "UI/LRSlateDraw.h"

void SLRChamberCamera::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	SetClipping(EWidgetClipping::ClipToBounds);
}

const FLRSimulation* SLRChamberCamera::GetSimulation() const
{
	const ULRGameSubsystem* Sub = Subsystem.Get();
	return Sub ? Sub->GetSimulation() : nullptr;
}

FVector2D SLRChamberCamera::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(320.0, 240.0);
}

int32 SLRChamberCamera::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FLRSimulation* Sim = GetSimulation();
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const FVector2f Centre(Size.X * 0.5f, Size.Y * 0.52f);
	const float Cage = FMath::Min(Size.X, Size.Y * 4.f / 3.f) * 0.3f;

	// A choppy clock: the picture only changes FramesPerSecond times a second.
	const double Clock = FMath::Fmod(FSlateApplication::Get().GetCurrentTime(), 3600.0);
	const float Time = static_cast<float>(FMath::FloorToDouble(Clock * FramesPerSecond) / FramesPerSecond);

	auto Polyline = [&](int32 Layer, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	};
	auto Ellipse = [&](int32 Layer, const FVector2f& At, float RadiusX, float RadiusY, const FLinearColor& Color, float Thickness)
	{
		constexpr int32 Segments = 48;
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.f * UE_PI * Index / Segments;
			Points.Add(At + FVector2f(FMath::Cos(Angle) * RadiusX, FMath::Sin(Angle) * RadiusY));
		}
		Polyline(Layer, Points, Color, Thickness);
	};

	// The host, if there is one: how close its 1 g sphere is to the wall (0..1).
	const bool bHost = Sim && Sim->GetData().Host.IsDefined() && !Sim->IsFrozen();
	const FLRHostDef* Host = Sim ? &Sim->GetData().Host : nullptr;
	const double Mass = bHost ? Sim->GetHostMass() : 0.0;
	const float Closeness = (bHost && Host->ChamberRadius > 0.f)
		? static_cast<float>(FMath::Clamp(Host->GetGravityRadius(Mass) / Host->ChamberRadius, 0.0, 1.0)) : 0.3f;

	// The containment cage: a wireframe sphere seen from a little above, slowly turning.
	FLinearColor CageColor(0.35f, 0.5f, 0.68f, 0.45f);
	if (Closeness >= 0.97f)
	{
		CageColor = FLinearColor(Style.Red.R, Style.Red.G, Style.Red.B, 0.8f);
	}
	else if (Closeness >= 0.85f)
	{
		CageColor = FLinearColor(Style.Orange.R, Style.Orange.G, Style.Orange.B, 0.65f);
	}
	for (const float Latitude : { -0.6f, -0.3f, 0.f, 0.3f, 0.6f })
	{
		const float Ring = Cage * FMath::Sqrt(1.f - Latitude * Latitude);
		Ellipse(LayerId, Centre + FVector2f(0.f, Cage * Latitude), Ring, Ring * 0.22f, CageColor, 1.f);
	}
	for (int32 Meridian = 0; Meridian < 4; ++Meridian)
	{
		const float Angle = Time * 0.35f + Meridian * UE_PI / 4.f;
		Ellipse(LayerId, Centre, Cage * FMath::Abs(FMath::Cos(Angle)), Cage, CageColor, 1.f);
	}
	LRSlateDraw::Circle(OutDrawElements, LayerId + 1, AllottedGeometry, Centre, Cage, CageColor, 2.f);

	// The injectors: four nozzles on the cage, and their beams while they run.
	const float Flow = Sim ? static_cast<float>(Sim->GetInjectorFlow()) : 0.f;
	const float MaxRate = (Host && Host->InjectorMaxRate > 0.0) ? static_cast<float>(Host->InjectorMaxRate) : 1.f;
	const float Strength = FMath::Clamp(FMath::Abs(Flow) / MaxRate, 0.f, 1.f);
	const FLinearColor BeamBase = Flow < 0.f ? Style.Vent : FLinearColor(0.45f, 1.f, 0.8f, 1.f);
	for (const float Degrees : { 25.f, 155.f, 205.f, 335.f })
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		const FVector2f Nozzle = Centre + FVector2f(FMath::Cos(Radians), FMath::Sin(Radians) * 0.8f) * Cage;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(6.f, 6.f), FSlateLayoutTransform(1.f, Nozzle - FVector2f(3.f, 3.f))),
			&Style.WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.6f, 0.65f, 0.75f, 0.9f));
		if (!bHost || Strength < 0.001f)
		{
			continue;
		}
		// Dashes stream in towards the singularity while feeding, and out while venting.
		constexpr int32 Dashes = 6;
		const float Travel = Time * (0.6f + Strength) * (Flow < 0.f ? -1.f : 1.f);
		const FLinearColor BeamColor(BeamBase.R, BeamBase.G, BeamBase.B, 0.3f + 0.7f * Strength);
		for (int32 Dash = 0; Dash < Dashes; ++Dash)
		{
			const float Start = FMath::Frac(static_cast<float>(Dash) / Dashes + Travel);
			const float End = FMath::Min(Start + 0.07f, 1.f);
			Polyline(LayerId + 2, { FMath::Lerp(Nozzle, Centre, Start), FMath::Lerp(Nozzle, Centre, End) }, BeamColor, 1.5f + 2.f * Strength);
		}
	}

	// The singularity: a pulsing glow in its Hawking colour, as big as its 1 g sphere is close
	// to the wall (brighter while venting drives it into stimulated emission).
	if (bHost)
	{
		const FLinearColor Glow = LRSlateDraw::HawkingGlow(Mass);
		const bool bVenting = Sim->IsVenting() || Flow < 0.f;
		const float Pulse = 1.f + (bVenting ? 0.2f : 0.08f) * FMath::Sin(Time * (bVenting ? 11.f : 4.f));
		const float Radius = Cage * (0.12f + 0.55f * Closeness) * Pulse;
		constexpr int32 Layers = 6;
		for (int32 Index = 0; Index < Layers; ++Index)
		{
			const float Fraction = 1.f - static_cast<float>(Index) / Layers;
			const float Alpha = (bVenting ? 0.12f : 0.07f) + 0.04f * Index;
			LRSlateDraw::Disc(OutDrawElements, LayerId + 3, AllottedGeometry, Centre, Radius * Fraction, FLinearColor(Glow.R, Glow.G, Glow.B, Alpha));
		}
		LRSlateDraw::Disc(OutDrawElements, LayerId + 4, AllottedGeometry, Centre, 3.f, FLinearColor::White);
	}
	else
	{
		LRSlateDraw::Label(OutDrawElements, LayerId + 4, AllottedGeometry, TEXT("NO HORIZON"), Style.SmallFont, Centre, Style.TextDim);
	}

	// CCTV dressing: scanlines, a blinking REC, and a timestamp.
	const FLinearColor Scanline(0.f, 0.f, 0.f, 0.18f);
	for (float Y = 1.f; Y < Size.Y; Y += 3.f)
	{
		Polyline(LayerId + 5, { FVector2f(0.f, Y), FVector2f(Size.X, Y) }, Scanline, 1.f);
	}
	if (FMath::Frac(Time * 0.5f) < 0.5f)
	{
		LRSlateDraw::Disc(OutDrawElements, LayerId + 6, AllottedGeometry, FVector2f(Size.X - 44.f, 13.f), 4.f, Style.Red);
	}
	LRSlateDraw::Label(OutDrawElements, LayerId + 6, AllottedGeometry, TEXT("REC"), Style.SmallFont, FVector2f(Size.X - 24.f, 13.f), Style.Red);
	LRSlateDraw::Label(OutDrawElements, LayerId + 6, AllottedGeometry, FDateTime::Now().ToString(TEXT("%Y-%m-%d  %H:%M:%S")), Style.SmallFont,
		FVector2f(78.f, Size.Y - 12.f), Style.TextDim);
	return LayerId + 6;
}
