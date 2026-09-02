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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance Portal")
	TObjectPtr<USphereComponent> Trigger = nullptr;

	// 눈에 보이라고 두는 것뿐이다. 충돌은 Trigger 가 전담한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Instance Portal")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	// 레벨 이동은 한 프레임에 끝나지 않는다. 그 사이에 겹침이 몇 번 더 들어와
	// EnterInstance 를 반복해서 부르는 것을 막는다.
	bool bTravelStarted = false;
};
