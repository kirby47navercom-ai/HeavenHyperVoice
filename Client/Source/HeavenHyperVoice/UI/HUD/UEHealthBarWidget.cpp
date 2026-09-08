#include "UEHealthBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

UUEHealthBarWidget::UUEHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("포켓몬"));
	HealthTextFormat = FText::FromString(TEXT("{0} / {1}"));
}

void UUEHealthBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshBoundWidgets();
}

void UUEHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimatingHealthDecrease)
	{
		return;
	}

	HealthDecreaseElapsed += FMath::Max(InDeltaTime, 0.0f);
	const float Alpha = HealthDecreaseDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(HealthDecreaseElapsed / HealthDecreaseDuration, 0.0f, 1.0f)
		: 1.0f;
	DisplayedHealth = FMath::Lerp(HealthDecreaseStart, CurrentHealth, Alpha);

	if (Alpha >= 1.0f)
	{
		DisplayedHealth = CurrentHealth;
		bAnimatingHealthDecrease = false;
	}

	RefreshBoundWidgets();
	BP_OnHealthChanged(DisplayedHealth, MaxHealth, GetHealthPercent());
}

void UUEHealthBarWidget::SetHealth(float InCurrentHealth, float InMaxHealth)
{
	MaxHealth = FMath::Max(InMaxHealth, 0.0f);
	const float NewHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);

	if (!bHealthInitialized || NewHealth >= DisplayedHealth)
	{
		DisplayedHealth = NewHealth;
		bAnimatingHealthDecrease = false;
	}
	else
	{
		HealthDecreaseStart = FMath::Min(DisplayedHealth, MaxHealth);
		DisplayedHealth = HealthDecreaseStart;
		HealthDecreaseElapsed = 0.0f;
		bAnimatingHealthDecrease = HealthDecreaseDuration > KINDA_SMALL_NUMBER;
	}

	CurrentHealth = NewHealth;
	bHealthInitialized = true;
	if (!bAnimatingHealthDecrease)
	{
		DisplayedHealth = CurrentHealth;
	}

	RefreshBoundWidgets();
	BP_OnHealthChanged(DisplayedHealth, MaxHealth, GetHealthPercent());
}

void UUEHealthBarWidget::SetDisplayName(const FText& InDisplayName)
{
	DisplayName = InDisplayName;
	RefreshBoundWidgets();
	BP_OnDisplayNameChanged(DisplayName);
}

float UUEHealthBarWidget::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? DisplayedHealth / MaxHealth : 0.0f;
}

void UUEHealthBarWidget::RefreshBoundWidgets()
{
	if (PokemonNameText)
	{
		PokemonNameText->SetText(DisplayName);
	}

	if (HealthBar)
	{
		HealthBar->SetPercent(GetHealthPercent());
	}

	if (HealthText)
	{
		HealthText->SetText(FText::Format(
			HealthTextFormat,
			FText::AsNumber(FMath::RoundToInt(DisplayedHealth)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}
