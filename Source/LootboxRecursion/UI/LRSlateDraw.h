#pragma once

#include "CoreMinimal.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Simulation/LRPhysics.h"

/**
 * Small drawing helpers shared by the HUD's painted widgets (SLRFeedDial, SLRChamberView,
 * SLRChamberCamera).
 * Kept in one named namespace because unity builds merge .cpp files, so per-file helpers in
 * anonymous namespaces with the same name collide.
 */
namespace LRSlateDraw
{
	/** Text centred on a point, in the widget's local space. */
	inline void Label(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text,
		const FSlateFontInfo& Font, const FVector2f& CentredAt, const FLinearColor& Color)
	{
		const FVector2f Size(FVector2D(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font)));
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(1.f, CentredAt - Size * 0.5f)),
			Text, Font, ESlateDrawEffect::None, Color);
	}

	/** A circle outline, in the widget's local space. */
	inline void Circle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Radius,
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

	/** A filled disc: a circle outline as thick as the disc's radius. */
	inline void Disc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Radius,
		const FLinearColor& Color)
	{
		Circle(Out, Layer, Geometry, Centre, Radius * 0.5f, Color, Radius);
	}

	/** Blue-white for a small, hot host; amber as it grows and cools (log of the Hawking temperature). */
	inline FLinearColor HawkingGlow(double Mass)
	{
		const double Hot = FMath::Clamp((FMath::LogX(10.0, FMath::Max(LRPhysics::HawkingTemperature(Mass), 1.0)) - 15.5) / (17.2 - 15.5), 0.0, 1.0);
		return FMath::Lerp(FLinearColor(1.f, 0.62f, 0.25f), FLinearColor(0.72f, 0.86f, 1.f), static_cast<float>(Hot));
	}
}
