#pragma once

#include "CoreMinimal.h"
#include "../../UEPokemonCharacter.h"
#include "UEPokemonBossCharacter.generated.h"

class UUEPokemonBossData;

/**
 * 맵에 보스 외형을 배치하기 위한 최소 캐릭터 기반이다.
 *
 * 지금 단계에서는 AI를 만들지 않는다. 따라서 AIControllerClass를 비우고 자동 빙의를 끈다.
 * 이 클래스가 하는 일은 BossData의 기본 폼을 메시와 캡슐에 적용하는 것뿐이다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPokemonBossCharacter : public AUEPokemonCharacter
{
	GENERATED_BODY()

public:
	AUEPokemonBossCharacter();

	/** 에디터에서 값을 바꾸면 배치된 보스 외형을 즉시 갱신한다. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 데이터 에셋의 현재 폼을 메시·애님 클래스·충돌 크기에 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Data")
	void ApplyBossData();

	UFUNCTION(BlueprintPure, Category = "Boss|Data")
	UUEPokemonBossData* GetBossData() const { return BossData; }

protected:
	virtual void BeginPlay() override;

private:
	// 기라티나 같은 개별 보스 데이터 에셋을 블루프린트 기본값으로 연결한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonBossData> BossData = nullptr;

	// NAME_None이면 BossData의 DefaultFormId를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Data", meta = (AllowPrivateAccess = "true"))
	FName InitialFormId = NAME_None;
};
