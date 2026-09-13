#include "UEGachaPool.h"

namespace
{
bool Eligible(const FUEGachaEntry& Entry)
{
	return Entry.DexNumber > 0 && static_cast<uint8>(Entry.Rarity) <= 2
		&& FMath::IsFinite(Entry.Weight) && Entry.Weight > 0;
}
}

FText UUEGachaPool::GetRarityLabel(EUEGachaRarity Rarity) const
{
	return Rarity == EUEGachaRarity::SuperRare ? SuperRareLabel : Rarity == EUEGachaRarity::Rare ? RareLabel : NormalLabel;
}

float UUEGachaPool::GetEntryProbability(int32 Index) const
{
	double Total = 0;
	for (const auto& Entry : Entries) if (Eligible(Entry)) Total += Entry.Weight;
	return Entries.IsValidIndex(Index) && Eligible(Entries[Index]) && Total > 0
		? static_cast<float>(Entries[Index].Weight / Total) : 0;
}

EUEGachaType UUEGachaPool::GetServerType() const
{
	if (ServerType != EUEGachaType::None)
	{
		return ServerType;
	}
	return DisplayOrder >= 0 && DisplayOrder < 5
		? static_cast<EUEGachaType>(DisplayOrder + 1)
		: EUEGachaType::None;
}

// 추첨은 서버가 한다. 여기 가중치는 화면의 확률 표시에만 쓴다.
const FUEGachaEntry* UUEGachaPool::FindByDex(int32 Dex) const
{
	for (const FUEGachaEntry& Entry : Entries)
	{
		if (Entry.DexNumber == Dex)
		{
			return &Entry;
		}
	}
	return nullptr;
}
