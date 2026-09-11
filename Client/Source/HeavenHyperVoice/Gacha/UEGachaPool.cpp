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

bool UUEGachaPool::Draw(FUEGachaEntry& Result) const
{
	double Total = 0;
	for (const auto& Entry : Entries) if (Eligible(Entry)) Total += Entry.Weight;
	if (Total <= 0) return false;
	double Target = FMath::FRand() * Total;
	const FUEGachaEntry* Last = nullptr;
	for (const auto& Entry : Entries)
	{
		if (!Eligible(Entry)) continue;
		Last = &Entry;
		Target -= Entry.Weight;
		if (Target < 0) { Result = Entry; return true; }
	}
	Result = *Last;
	return true;
}
