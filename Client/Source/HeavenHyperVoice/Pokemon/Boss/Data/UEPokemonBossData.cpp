#include "UEPokemonBossData.h"

const FUEPokemonBossFormData* UUEPokemonBossData::FindForm(FName FormId) const
{
	// 호출자가 폼을 지정하지 않았으면 데이터 에셋에 설정된 기본 폼을 찾는다.
	const FName RequestedFormId = FormId.IsNone() ? DefaultFormId : FormId;

	if (!RequestedFormId.IsNone())
	{
		for (const FUEPokemonBossFormData& Form : Forms)
		{
			if (Form.FormId == RequestedFormId)
			{
				return &Form;
			}
		}
	}

	// 잘못된 ID 때문에 보스가 빈 메시로 나타나지 않도록 첫 폼을 최후 기본값으로 쓴다.
	return Forms.IsEmpty() ? nullptr : &Forms[0];
}

bool UUEPokemonBossData::GetForm(FName FormId, FUEPokemonBossFormData& OutForm) const
{
	const FUEPokemonBossFormData* FoundForm = FindForm(FormId);
	if (!FoundForm)
	{
		return false;
	}

	OutForm = *FoundForm;
	return true;
}
