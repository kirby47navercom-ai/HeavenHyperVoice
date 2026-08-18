#pragma once

#include "CoreMinimal.h"
#include "../Data/UEHHVCustomizationTypes.h"

namespace UEHHVCustomizationWidgetPrivate
{
	inline const FLinearColor PanelColor(0.025f, 0.035f, 0.045f, 0.96f);
	inline const FLinearColor PanelSecondary(0.055f, 0.065f, 0.075f, 0.98f);
	inline const FLinearColor TileColor(0.095f, 0.11f, 0.125f, 1.0f);
	inline const FLinearColor TextColor(0.94f, 0.95f, 0.96f, 1.0f);
	inline const FLinearColor MutedTextColor(0.62f, 0.66f, 0.70f, 1.0f);
	inline const FLinearColor AccentColor(0.13f, 0.72f, 0.66f, 1.0f);
	inline const FLinearColor SelectedColor(0.92f, 0.55f, 0.16f, 1.0f);

	constexpr int32 MaxVisibleBodyEquipmentOptions = 14;
	constexpr int32 FirstVisibleBodyEquipmentIndex = 1;

	inline void AddUniqueColor(TArray<FLinearColor>& Colors, const FLinearColor& Color)
	{
		const FLinearColor Clamped = Color.GetClamped();
		for (const FLinearColor& Existing : Colors)
		{
			if (Existing.Equals(Clamped, 0.015f))
			{
				return;
			}
		}
		Colors.Add(Clamped);
	}

	inline void AppendVisualPickerColors(TArray<FLinearColor>& Colors, EUEHHVColorChannel Channel)
	{
		if (Channel == EUEHHVColorChannel::Skin)
		{
			for (float Value : {1.0f, 0.92f, 0.84f, 0.76f, 0.66f, 0.56f})
			{
				for (float Saturation : {0.12f, 0.22f, 0.34f, 0.46f, 0.58f, 0.70f})
				{
					AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(22, uint8(Saturation * 255.0f), uint8(Value * 255.0f)));
				}
			}
			return;
		}

		if (Channel == EUEHHVColorChannel::Hair)
		{
			for (float Value : {0.18f, 0.32f, 0.48f, 0.68f, 0.86f})
			{
				for (uint8 Hue : {0, 14, 24, 34, 48, 64, 96, 128, 160, 190, 216, 238})
				{
					AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(Hue, 185, uint8(Value * 255.0f)));
				}
			}
			return;
		}

		for (float Value : {0.30f, 0.46f, 0.62f, 0.78f})
		{
			for (uint8 Hue : {0, 24, 42, 74, 105, 135, 160, 186, 210, 232})
			{
				AddUniqueColor(Colors, FLinearColor::MakeFromHSV8(Hue, 180, uint8(Value * 255.0f)));
			}
		}
		AddUniqueColor(Colors, FLinearColor(0.08f, 0.08f, 0.08f));
		AddUniqueColor(Colors, FLinearColor(0.72f, 0.72f, 0.72f));
	}

	struct FCategoryEntry
	{
		EUEHHVCustomizationCategory Category;
		const TCHAR* Label;
	};

	inline TConstArrayView<FCategoryEntry> GetCategoryEntries()
	{
		static const TArray<FCategoryEntry> Entries = {
			{EUEHHVCustomizationCategory::Body, TEXT("몸")},
			{EUEHHVCustomizationCategory::Head, TEXT("얼굴")},
			{EUEHHVCustomizationCategory::Hair, TEXT("머리카락")},
			{EUEHHVCustomizationCategory::Eyes, TEXT("눈")},
			{EUEHHVCustomizationCategory::BodyEquipment, TEXT("의상")}
		};
		return Entries;
	}
}
