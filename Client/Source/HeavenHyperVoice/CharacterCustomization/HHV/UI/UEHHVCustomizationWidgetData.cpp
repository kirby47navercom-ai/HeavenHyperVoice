#include "UEHHVCustomizationWidget.h"
#include "UEHHVCustomizationWidgetPrivate.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"

using namespace UEHHVCustomizationWidgetPrivate;
int32 UUEHHVCustomizationWidget::GetSelectedIndex(EUEHHVCustomizationCategory Category) const
{
	const FUEHHVAppearance& Appearance = GetAppearance();
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body:
		return Appearance.BodyIndex;
	case EUEHHVCustomizationCategory::Head:
		return Appearance.HeadIndex;
	case EUEHHVCustomizationCategory::Hair:
		return Appearance.HairIndex;
	case EUEHHVCustomizationCategory::Eyes:
		return Appearance.EyeIndex;
	case EUEHHVCustomizationCategory::BodyEquipment:
		return Appearance.BodyEquipmentIndex;
	default:
		return 0;
	}
}

int32 UUEHHVCustomizationWidget::GetVisibleOptionCount(EUEHHVCustomizationCategory Category) const
{
	const int32 RawCount = GetOptionCount(Category);
	if (Category == EUEHHVCustomizationCategory::Body && RawCount > 2)
	{
		return 2;
	}
	if (Category == EUEHHVCustomizationCategory::BodyEquipment && RawCount > 1)
	{
		// 커마 화면에서 실제로 다룰 의상은 앞쪽 14개까지만 보여준다.
		return FMath::Min(RawCount - FirstVisibleBodyEquipmentIndex, MaxVisibleBodyEquipmentOptions);
	}
	return RawCount;
}

int32 UUEHHVCustomizationWidget::GetActualOptionIndex(
	EUEHHVCustomizationCategory Category,
	int32 VisibleIndex) const
{
	if (Category == EUEHHVCustomizationCategory::Body && GetOptionCount(Category) > 2)
	{
		return FMath::Clamp(VisibleIndex + 1, 1, GetOptionCount(Category) - 1);
	}
	if (Category == EUEHHVCustomizationCategory::BodyEquipment && GetOptionCount(Category) > 1)
	{
		const int32 LastVisibleActualIndex = FMath::Min(GetOptionCount(Category) - 1, MaxVisibleBodyEquipmentOptions);
		return FMath::Clamp(
			VisibleIndex + FirstVisibleBodyEquipmentIndex,
			FirstVisibleBodyEquipmentIndex,
			LastVisibleActualIndex);
	}
	return VisibleIndex;
}

FLinearColor UUEHHVCustomizationWidget::GetChannelColor(EUEHHVColorChannel Channel) const
{
	const FUEHHVAppearance& Appearance = GetAppearance();
	switch (Channel)
	{
	case EUEHHVColorChannel::Skin:
		return Appearance.SkinColor;
	case EUEHHVColorChannel::Hair:
		return Appearance.HairColor;
	case EUEHHVColorChannel::Eye:
		return Appearance.EyeColor;
	default:
		return FLinearColor::White;
	}
}

TArray<FLinearColor> UUEHHVCustomizationWidget::GetPaletteColors(EUEHHVColorChannel Channel) const
{
	TArray<FLinearColor> Colors;
	auto AppendCatalogColors = [&Colors](const TArray<FLinearColor>& SourceColors)
	{
		for (const FLinearColor& Color : SourceColors)
		{
			AddUniqueColor(Colors, Color);
		}
	};

	if (Catalog)
	{
		switch (Channel)
		{
		case EUEHHVColorChannel::Skin:
			if (!Catalog->SkinColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->SkinColors);
			}
			break;
		case EUEHHVColorChannel::Hair:
			if (!Catalog->HairColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->HairColors);
			}
			break;
		case EUEHHVColorChannel::Eye:
			if (!Catalog->EyeColors.IsEmpty())
			{
				AppendCatalogColors(Catalog->EyeColors);
			}
			break;
		default:
			break;
		}
	}

	AppendVisualPickerColors(Colors, Channel);
	if (!Colors.IsEmpty())
	{
		return Colors;
	}

	if (Channel == EUEHHVColorChannel::Skin)
	{
		return {
			FLinearColor(1.0f, 0.74f, 0.62f),
			FLinearColor(0.90f, 0.55f, 0.40f),
			FLinearColor(0.62f, 0.36f, 0.24f),
			FLinearColor(0.36f, 0.22f, 0.16f)
		};
	}
	if (Channel == EUEHHVColorChannel::Hair)
	{
		return {
			FLinearColor(0.05f, 0.035f, 0.025f),
			FLinearColor(0.22f, 0.10f, 0.045f),
			FLinearColor(0.65f, 0.32f, 0.12f),
			FLinearColor(0.84f, 0.62f, 0.38f),
			FLinearColor(0.58f, 0.08f, 0.08f),
			FLinearColor(0.28f, 0.42f, 0.72f)
		};
	}
	return {
		FLinearColor(0.18f, 0.40f, 0.22f),
		FLinearColor(0.18f, 0.36f, 0.62f),
		FLinearColor(0.48f, 0.32f, 0.18f),
		FLinearColor(0.70f, 0.72f, 0.72f),
		FLinearColor(0.08f, 0.08f, 0.08f)
	};
}

FString UUEHHVCustomizationWidget::GetCategoryLabel(EUEHHVCustomizationCategory Category)
{
	switch (Category)
	{
	case EUEHHVCustomizationCategory::Body:
		return TEXT("몸");
	case EUEHHVCustomizationCategory::Head:
		return TEXT("얼굴");
	case EUEHHVCustomizationCategory::Hair:
		return TEXT("머리카락");
	case EUEHHVCustomizationCategory::Eyes:
		return TEXT("눈");
	case EUEHHVCustomizationCategory::BodyEquipment:
		return TEXT("의상");
	default:
		return TEXT("항목");
	}
}

