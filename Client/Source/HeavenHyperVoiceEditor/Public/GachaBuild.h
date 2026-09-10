#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GachaBuild.generated.h"

/** 편집 가능한 BP, WBP, 데이터 에셋, 메시, 레벨을 한 번 생성하는 제작 도구다. */
UCLASS()
class HEAVENHYPERVOICEEDITOR_API UHHVGachaBuildCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHHVGachaBuildCommandlet();
	virtual int32 Main(const FString& Params) override;
};
