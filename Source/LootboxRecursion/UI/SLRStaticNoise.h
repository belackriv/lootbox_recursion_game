#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 * Old-school TV static (docs/DESIGN.md, "The instruments go down"): random grey noise that
 * redraws about 30 times a second, with a brighter band rolling down it. It draws nothing while
 * Intensity is 0, so it can sit over the viewport all the time.
 *
 * The noise is a small transient texture stretched over the widget (nearest filtering, so each
 * texel is a visible speck). If the M_Static hook exists it's drawn with that material instead,
 * which gets the noise as Texture and the intensity as Amount (docs/MATERIALS.md).
 */
class LOOTBOXRECURSION_API SLRStaticNoise : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLRStaticNoise)
		: _Intensity(1.f)
		, _Resolution(FIntPoint(320, 180))
		, _Material(nullptr)
		{}
		/** 0 = off, 1 = full static. */
		SLATE_ATTRIBUTE(float, Intensity)
		/** The noise texture's size in texels: fewer means coarser specks. */
		SLATE_ARGUMENT(FIntPoint, Resolution)
		/** The M_Static hook, or nullptr for the plain noise. */
		SLATE_ARGUMENT(UMaterialInterface*, Material)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/** How often the noise redraws. */
	static constexpr double FramesPerSecond = 30.0;

private:
	/** Fill the texture with fresh noise. */
	void Regenerate();
	/** A fast pseudo-random number (xorshift): static doesn't need a good generator. */
	uint32 NextRandom();

	TAttribute<float> Intensity;
	FIntPoint Resolution = FIntPoint(320, 180);
	TStrongObjectPtr<UTexture2D> Texture;
	TStrongObjectPtr<UMaterialInstanceDynamic> Material;
	FSlateBrush Brush;
	TArray<FColor> Pixels;
	uint32 NoiseState = 0x9E3779B9u;
	double Clock = 0.0;
	double NextFrameAt = 0.0;
	/** The whole picture's brightness wobbles a little from frame to frame. */
	float Flicker = 1.f;
};
