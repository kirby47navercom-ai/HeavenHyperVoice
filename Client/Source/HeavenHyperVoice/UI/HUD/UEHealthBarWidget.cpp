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

void UUEHealthBarWidget::SetHealth(float InCurrentHealth, float InMaxHealth)
{
	MaxHealth = FMath::Max(InMaxHealth, 0.0f);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);

	RefreshBoundWidgets();
	BP_OnHealthChanged(CurrentHealth, MaxHealth, GetHealthPercent());
}

void UUEHealthBarWidget::SetDisplayName(const FText& InDisplayName)
{
	DisplayName = InDisplayName;
	RefreshBoundWidgets();
	BP_OnDisplayNameChanged(DisplayName);
}

float UUEHealthBarWidget::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
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
			FText::AsNumber(FMath::RoundToInt(CurrentHealth)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}
