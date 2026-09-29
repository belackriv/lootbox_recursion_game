#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

/**
 * Colours, brushes and fonts for the HUD: cool greys and an amber accent, matching the
 * pocket-universe theme (the Rails app used a brown "Factorio" palette). Slate keeps pointers to brushes and
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
	/** The accent colour (accretion-disk amber). */
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

	/*
	 * The outside panel is the facility's console, the inverse of the inside: light grey, with
	 * dark text, and its instruments (the dial, the chamber) set in dark screens.
	 */
	FLinearColor ConsolePanel;
	FLinearColor ConsoleInner;
	FLinearColor ConsoleBorder;
	FLinearColor ConsoleText;
	FLinearColor ConsoleTextDim;
	/** A darker amber that reads on light grey. */
	FLinearColor ConsoleAccent;
	FLinearColor ConsoleScreen;
	FLinearColor ConsoleScreenBorder;

	/** One white brush, tinted per use via BorderBackgroundColor. */
	FSlateColorBrush WhiteBrush;

	FButtonStyle ButtonStyle;
	FButtonStyle ConsoleButtonStyle;
	FButtonStyle SlotButtonStyle;
	FProgressBarStyle ProgressStyle;

	FSlateFontInfo TitleFont;
	FSlateFontInfo HeadingFont;
	FSlateFontInfo BodyFont;
	FSlateFontInfo SmallFont;

private:
	FLRHudStyle();
};
