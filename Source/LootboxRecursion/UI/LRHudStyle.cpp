#include "UI/LRHudStyle.h"

#include "Styling/CoreStyle.h"

namespace
{
	FLinearColor Hex(const TCHAR* InHex)
	{
		return FLinearColor(FColor::FromHex(InHex));
	}
}

const FLRHudStyle& FLRHudStyle::Get()
{
	static const FLRHudStyle Instance;
	return Instance;
}

FLRHudStyle::FLRHudStyle()
	: Background(Hex(TEXT("1B1A17")))
	, Panel(Hex(TEXT("2D2719")))
	, PanelInner(Hex(TEXT("252015")))
	, Border(Hex(TEXT("5A4A30")))
	, BorderLight(Hex(TEXT("8B7355")))
	, Orange(Hex(TEXT("E8A020")))
	, Text(Hex(TEXT("D4C5A0")))
	, TextDim(Hex(TEXT("8B7A5E")))
	, TextDark(Hex(TEXT("4A3F2F")))
	, Button(Hex(TEXT("4A3F2F")))
	, ButtonHover(Hex(TEXT("6B5A3E")))
	, ButtonDisabled(Hex(TEXT("2A2218")))
	, SlotFill(Hex(TEXT("1A1410")))
	, SlotBorder(Hex(TEXT("3D3322")))
	, Red(Hex(TEXT("C0392B")))
	, Green(Hex(TEXT("4CAF50")))
	, WhiteBrush(FLinearColor::White)
{
	ButtonStyle = FButtonStyle()
		.SetNormal(FSlateColorBrush(Button))
		.SetHovered(FSlateColorBrush(ButtonHover))
		.SetPressed(FSlateColorBrush(FLinearColor(Orange.R * 0.6f, Orange.G * 0.6f, Orange.B * 0.6f, 1.f)))
		.SetDisabled(FSlateColorBrush(ButtonDisabled))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	SlotButtonStyle = FButtonStyle()
		.SetNormal(FSlateColorBrush(SlotFill))
		.SetHovered(FSlateColorBrush(PanelInner))
		.SetPressed(FSlateColorBrush(Button))
		.SetDisabled(FSlateColorBrush(SlotFill))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	ProgressStyle = FProgressBarStyle()
		.SetBackgroundImage(FSlateColorBrush(SlotFill))
		.SetFillImage(FSlateColorBrush(FLinearColor::White))
		.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));

	TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	HeadingFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
}
