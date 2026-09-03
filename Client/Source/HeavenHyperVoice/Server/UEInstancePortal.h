#pragma once

// 인스턴스 입구.
//
// 밟으면 인스턴스로 들어가고, 인스턴스 쪽에 같은 것을 놓고 InstanceType 을 0
// 으로 두면 필드로 나오는 출구가 된다. 방향이 둘뿐이라 액터도 하나면 된다.
//
// 에셋 없이 레벨에 바로 끌어다 놓을 수 있다 — 트리거 구체와 눈에 보이는
// 원기둥을 생성자가 직접 만든다. 미술 에셋이 나오면 Mesh 만 바꾸면 된다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "UEInstancePortal.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class HEAVENHYPERVOICE_API AUEInstancePortal : public AActor
{
	GENERATED_BODY()

public:
	AUEInstancePortal();

protected:
	virtual void BeginPlay() override;

	/**
	 * 들어갈 인스턴스 종류. 서버의 --instance-types 목록에 있어야 한다.
	 *
	 * 0 이면 반대로 동작한다 — 인스턴스에서 필드로 나가는 출구가 된다.
	 * 인스턴스 맵에 놓는 포탈이 이 값을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Portal")
	int32 InstanceType = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Portal", meta = (ClampMin = "50.0"))
	float TriggerRadius = 200.0f;

	/**
	 * 인스턴스로 들어가기 직전에 캐릭터를 포탈 중심에서 이만큼 떨어뜨린다.
	 *
	 * 필드 서버는 접속이 끊길 때 마지막 좌표를 저장한다. 포탈 위에 선 채로
	 * 떠나면 다음 접속에 그 자리에서 살아나고, 겹침이 다시 터져 인스턴스로
	 * 끌려 들어간다. 그래서 트리거 밖으로 밀어낸 자리를 저장시킨다.
	 *
	 * 서버가 속도 상한으로 이동을 자르므로 너무 크게 잡지 말 것. 한 번에
	 * 200uu 남짓이 안전하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Portal", meta = (ClampMin = "0.0"))
	float ExitMargin = 150.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance Portal")
	TObjectPtr<USphereComponent> Trigger = nullptr;

	// 눈에 보이라고 두는 것뿐이다. 충돌은 Trigger 가 전담한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance Portal")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

private:
	// 필드를 떠나기 전에 캐릭터를 트리거 밖으로 옮긴다.
	void PushOutOfTrigger(AActor* PlayerActor) const;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	// 레벨 이동은 한 프레임에 끝나지 않는다. 그 사이에 겹침이 몇 번 더 들어와
	// EnterInstance 를 반복해서 부르는 것을 막는다.
	bool bTravelStarted = false;
};
