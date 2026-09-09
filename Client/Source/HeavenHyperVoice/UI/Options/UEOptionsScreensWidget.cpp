#include "UEOptionsScreensWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"

void UUEOptionsScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::GoBack);
}

void UUEOptionsScreenWidget::GoBack()
{
    OnScreenActionRequested.Broadcast(TEXT("Back"));
}

FReply UUEOptionsScreenWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Escape)
    {
        if (!KeyEvent.IsRepeat()) GoBack();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UUEGameSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ApplySettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplySettings);
    LoadSettings();
    DisplayTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowDisplay);
    GraphicsTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowGraphics);
    QualityCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnPresetChanged);
    for (UComboBoxString* Combo : {ShadowCombo.Get(), TextureCombo.Get(), EffectsCombo.Get(),
        ViewDistanceCombo.Get(), AntiAliasingCombo.Get(), FoliageCombo.Get()})
    {
        Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnDetailChanged);
    }
    ShowDisplay();
}

void UUEGameSettingsWidget::SelectCategory(int32 Index)
{
    SettingsPages->SetActiveWidgetIndex(Index);
    CategoryTitleText->SetText(FText::FromString(Index == 0 ? TEXT("화면") : TEXT("그래픽")));
    const FLinearColor Selected(0.35f, 0.80f, 0.70f, 1.0f);
    DisplayTab->SetBackgroundColor(Index == 0 ? Selected : FLinearColor::White);
    GraphicsTab->SetBackgroundColor(Index == 1 ? Selected : FLinearColor::White);
}

void UUEGameSettingsWidget::ShowDisplay() { SelectCategory(0); }
void UUEGameSettingsWidget::ShowGraphics() { SelectCategory(1); }

void UUEGameSettingsWidget::OnPresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    const int32 Quality = QualityCombo->GetSelectedIndex() - 1;
    if (bUpdatingQuality || Quality < 0) return;
    bUpdatingQuality = true;
    for (UComboBoxString* Combo : {ShadowCombo.Get(), TextureCombo.Get(), EffectsCombo.Get(),
        ViewDistanceCombo.Get(), AntiAliasingCombo.Get(), FoliageCombo.Get()})
    {
        Combo->SetSelectedIndex(Quality);
    }
    bUpdatingQuality = false;
    SettingsStatusText->SetText(FText::GetEmpty());
}

void UUEGameSettingsWidget::OnDetailChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingQuality) return;
    QualityCombo->SetSelectedIndex(0);
    SettingsStatusText->SetText(FText::GetEmpty());
}

void UUEGameSettingsWidget::LoadSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;
	bUpdatingQuality = true;
	QualityCombo->SetSelectedIndex(Settings->GetOverallScalabilityLevel() + 1);
	ShadowCombo->SetSelectedIndex(Settings->GetShadowQuality());
	TextureCombo->SetSelectedIndex(Settings->GetTextureQuality());
	EffectsCombo->SetSelectedIndex(Settings->GetVisualEffectQuality());
	ViewDistanceCombo->SetSelectedIndex(Settings->GetViewDistanceQuality());
	AntiAliasingCombo->SetSelectedIndex(Settings->GetAntiAliasingQuality());
	FoliageCombo->SetSelectedIndex(Settings->GetFoliageQuality());
	bUpdatingQuality = false;
	FrameLimitCombo->ClearOptions();
	for (const TCHAR* Label : {TEXT("제한 없음"), TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144")})
	{
		FrameLimitCombo->AddOption(Label);
	}
	const float Limit = Settings->GetFrameRateLimit();
	const FString LimitLabel = Limit <= 0.0f ? TEXT("제한 없음") : FString::SanitizeFloat(Limit, 0);
	if (FrameLimitCombo->FindOptionIndex(LimitLabel) == INDEX_NONE) FrameLimitCombo->AddOption(LimitLabel);
	FrameLimitCombo->SetSelectedOption(LimitLabel);
	VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
	SettingsStatusText->SetText(FText::GetEmpty());
}

void UUEGameSettingsWidget::ApplySettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;
	const int32 Quality = QualityCombo->GetSelectedIndex() - 1;
	if (Quality >= 0) Settings->SetOverallScalabilityLevel(Quality);
	Settings->SetShadowQuality(ShadowCombo->GetSelectedIndex());
	Settings->SetTextureQuality(TextureCombo->GetSelectedIndex());
	Settings->SetVisualEffectQuality(EffectsCombo->GetSelectedIndex());
	Settings->SetViewDistanceQuality(ViewDistanceCombo->GetSelectedIndex());
	Settings->SetAntiAliasingQuality(AntiAliasingCombo->GetSelectedIndex());
	Settings->SetFoliageQuality(FoliageCombo->GetSelectedIndex());
	Settings->SetFrameRateLimit(FrameLimitCombo->GetSelectedIndex() == 0
		? 0.0f : FCString::Atof(*FrameLimitCombo->GetSelectedOption()));
	Settings->SetVSyncEnabled(VSyncCheckBox->IsChecked());
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
	SettingsStatusText->SetText(FText::FromString(TEXT("설정을 저장했어.")));
}

void UUEOptionsConfirmWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Confirm);
}

void UUEOptionsConfirmWidget::SetAction(FName ActionId)
{
    PendingAction = ActionId;
    const bool bLogout = ActionId == TEXT("Logout");
    ConfirmTitleText->SetText(FText::FromString(bLogout ? TEXT("로그아웃") : TEXT("캐릭터 선택")));
    ConfirmMessageText->SetText(FText::FromString(bLogout
        ? TEXT("현재 계정에서 로그아웃하고 타이틀로 돌아갈까요?")
        : TEXT("캐릭터 선택으로 돌아갈까요?\n온라인 접속 중에는 다시 로그인해야 합니다.")));
}

void UUEOptionsConfirmWidget::Confirm()
{
    if (PendingAction == TEXT("Logout") || PendingAction == TEXT("CharacterSelect"))
    {
        OnScreenActionRequested.Broadcast(PendingAction);
    }
}
