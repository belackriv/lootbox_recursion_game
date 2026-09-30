#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SEditableTextBox;
class ULRGameSubsystem;
class ULRUserSettings;

/**
 * The Esc menu (docs/ROADMAP.md, M2.5): Resume, New Game, Save, Load, Settings, Quit. The HUD
 * shows it over everything and pauses the game while it's open.
 *
 * - Save: a new named save, or over an existing one. Load: any save, the autosave first.
 * - Settings: rebind every keyboard command (two keys each; click, then press a key; Esc
 *   cancels, Backspace clears), camera speeds and invert look, UI scale, graphics quality.
 *   Changes apply at once and are saved (ULRUserSettings; graphics in UGameUserSettings).
 *
 * It takes keyboard focus while open, so key presses reach it (for rebinding) rather than
 * the game.
 */
class LOOTBOXRECURSION_API SLRGameMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLRGameMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
		/** Resume: the HUD closes the menu (and unpauses). */
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Show the main page and take keyboard focus. */
	void Open();
	/** Esc: cancel a key capture, else go back a page, else close. */
	void Back();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EPage : uint8
	{
		Main,
		Save,
		Load,
		Settings,
		ConfirmNewGame,
		ConfirmQuit,
	};

	void ShowPage(EPage NewPage);
	TSharedRef<SWidget> BuildMainPage();
	TSharedRef<SWidget> BuildSavePage();
	TSharedRef<SWidget> BuildLoadPage();
	TSharedRef<SWidget> BuildSettingsPage();
	TSharedRef<SWidget> BuildConfirmPage(const FText& Question, TFunction<void()> OnYes);

	TSharedRef<SWidget> MakeMenuButton(const FText& Label, TFunction<void()> OnClick, float Width = 260.f);
	TSharedRef<SWidget> MakeHeading(const FText& Text);
	/** A button showing a command's key; click it, then press the new key. */
	TSharedRef<SWidget> MakeKeyButton(FName Command, int32 Index);
	TSharedRef<SWidget> MakeSliderRow(const FText& Label, float Min, float Max, TFunction<float(const ULRUserSettings&)> Get,
		TFunction<void(ULRUserSettings&, float)> Set);

	ULRUserSettings* GetSettings() const;
	/** Apply the settings now (camera, keys, UI scale); bSave also writes them to disk. */
	void SettingsChanged(bool bSave);

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;
	FSimpleDelegate OnClose;
	EPage Page = EPage::Main;
	TSharedPtr<SBox> PageBox;
	TSharedPtr<SEditableTextBox> SaveNameBox;
	/** The binding waiting for a key press, if any. */
	FName CaptureCommand;
	int32 CaptureIndex = 0;
	/** A line under the page: "Saved.", "Couldn't load that save." */
	FString Notice;
};
