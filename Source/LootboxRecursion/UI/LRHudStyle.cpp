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
	// Cosmic palette: cool blue-greys, translucent panels over the void, and the amber-gold
	// of an accretion disk as the accent. (Was the Rails app's brown "Factorio" palette.)
	: Background(Hex(TEXT("0B0D12")))
	, Panel(Hex(TEXT("151922")).CopyWithNewOpacity(0.9f))
	, PanelInner(Hex(TEXT("10131A")).CopyWithNewOpacity(0.92f))
	, Border(Hex(TEXT("2A3142")))
	, BorderLight(Hex(TEXT("4A5368")))
	, Orange(Hex(TEXT("F2A93B")))
	, Text(Hex(TEXT("D6DCE8")))
	, TextDim(Hex(TEXT("8A93A6")))
	, TextDark(Hex(TEXT("4A5263")))
	, Button(Hex(TEXT("232938")))
	, ButtonHover(Hex(TEXT("323A50")))
	, ButtonDisabled(Hex(TEXT("161A23")))
	, SlotFill(Hex(TEXT("0E1117")))
	, SlotBorder(Hex(TEXT("262C3A")))
	, Red(Hex(TEXT("E0525A")))
	, Green(Hex(TEXT("4FC38A")))
	, Selection(Hex(TEXT("8FD3FF")))
	, ConsolePanel(Hex(TEXT("C9CED7")).CopyWithNewOpacity(0.97f))
	, ConsoleInner(Hex(TEXT("B3BAC6")))
	, ConsoleBorder(Hex(TEXT("7B8394")))
	, ConsoleText(Hex(TEXT("1B2130")))
	, ConsoleTextDim(Hex(TEXT("4B5364")))
	, ConsoleAccent(Hex(TEXT("9A5800")))
	, ConsoleScreen(Hex(TEXT("0E1117")))
	, ConsoleScreenBorder(Hex(TEXT("3A4150")))
	, VentScreen(Hex(TEXT("2A0A26")))
	, Vent(Hex(TEXT("FF5FD2")))
	, WhiteBrush(FLinearColor::White)
{
	ButtonStyle = FButtonStyle()
		.SetNormal(FSlateColorBrush(Button))
		.SetHovered(FSlateColorBrush(ButtonHover))
		.SetPressed(FSlateColorBrush(FLinearColor(Orange.R * 0.45f, Orange.G * 0.45f, Orange.B * 0.45f, 1.f)))
		.SetDisabled(FSlateColorBrush(ButtonDisabled))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	ConsoleButtonStyle = FButtonStyle()
		.SetNormal(FSlateColorBrush(Hex(TEXT("A8AFBC"))))
		.SetHovered(FSlateColorBrush(Hex(TEXT("BAC0CB"))))
		.SetPressed(FSlateColorBrush(Hex(TEXT("D8A24A"))))
		.SetDisabled(FSlateColorBrush(Hex(TEXT("BFC3CA"))))
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
