#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

/**
 * Colours, brushes and fonts for the HUD - the Slate equivalent of the Rails app's
 * application.css (--color-fac-* "Factorio" palette). Slate keeps pointers to brushes and
 * styles, so they live in this long-lived singleton.
 */
struct FLRHudStyle
{
	static const FLRHudStyle& Get();

	FLinearColor Background;
	FLinearColor Panel;
	FLinearColor PanelInner;
	FLinearColor Border;
	FLinearColor BorderLight;
	FLinearColor Orange;
	FLinearColor Text;
	FLinearColor TextDim;
	FLinearColor TextDark;
	FLinearColor Button;
	FLinearColor ButtonHover;
	FLinearColor ButtonDisabled;
	FLinearColor SlotFill;
	FLinearColor SlotBorder;
	FLinearColor Red;
	FLinearColor Green;

	/** One white brush, tinted per use via BorderBackgroundColor. */
	FSlateColorBrush WhiteBrush;

	FButtonStyle ButtonStyle;
	FButtonStyle SlotButtonStyle;
	FProgressBarStyle ProgressStyle;

	FSlateFontInfo TitleFont;
	FSlateFontInfo HeadingFont;
	FSlateFontInfo BodyFont;
	FSlateFontInfo SmallFont;

private:
	FLRHudStyle();
};
