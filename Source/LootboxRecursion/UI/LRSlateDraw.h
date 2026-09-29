#pragma once

#include "CoreMinimal.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

/**
 * Small drawing helpers shared by the HUD's painted widgets (SLRFeedDial, SLRChamberView).
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
}
