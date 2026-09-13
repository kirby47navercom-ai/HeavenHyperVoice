#pragma once

// 뽑기 조작 한 벌. 컨트롤러에 붙여서 쓴다.
//
// 기계 목록·카메라·손잡이 드래그·화면이 전부 여기 있다. 뽑기방 전용
// 컨트롤러(AUEGachaStudioController)와 필드 컨트롤러(AUEPlayerController)가
// 같은 조작을 필요로 하는데, 컨트롤러는 하나만 붙으므로 상속으로는 나눌 수 없다.
//
// 뽑기방과 필드의 차이는 열고 닫는 방식뿐이다. 뽑기방은 들어가는 순간 열려
// 있고 닫지 않는다. 필드는 O 로 켰다 껐다 하며, 닫을 때 카메라와 입력 모드를
// 되돌린다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"

#include "UEGachaDesk.generated.h"

class AUEGachaMachine;
class APlayerController;
class UUEGachaStudioWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEGachaDeskComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEGachaDeskComponent();

	/**
	 * 뽑기 모드로 들어간다.
	 *
	 * 레벨에 놓인 기계가 있으면 그것을 쓰고(뽑기방이 이쪽이다), 없으면 화면
	 * 밖에 다섯 대를 띄운다. 필드에 기계를 배치하지 않아도 어디서나 열린다 —
	 * 카메라가 기계로 옮겨가므로 어디에 있든 보이는 것은 같다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "Gacha")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "Gacha")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Gacha")
	bool IsOpen() const { return bOpen; }

	/**
	 * 타입 버튼이 부른다. 뽑는 중에는 무시된다.
	 *
	 * 열 때는 블렌드 없이 바로 잡는다. 띄운 기계가 멀리 있어서 블렌드를 주면
	 * 카메라가 거기까지 날아가는 것이 그대로 보인다.
	 */
	void SelectMachine(int32 Index, float BlendTime = .6f);

	AUEGachaMachine* GetMachine() const { return CurrentMachine; }

	// 손잡이 드래그. 소유 컨트롤러가 마우스 버튼에 걸어 준다.
	void GrabHandle();
	void ReleaseHandle();

	/**
	 * 띄울 화면. 비어 있으면 /Game/Gacha/UI/WBP_GachaStudio 를 쓴다.
	 *
	 * 폴백이 있는 이유는 필드 컨트롤러 블루프린트를 손대지 않아도 O 가
	 * 동작하게 하려는 것이다. 다른 화면을 쓰려면 여기서 지정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gacha")
	TSubclassOf<UUEGachaStudioWidget> WidgetClass;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* Controller() const;

	// 레벨에 기계가 없을 때 다섯 대를 띄운다. 실패하면 false.
	bool SpawnMachines();

	// 손잡이 중심을 기준으로 한 마우스 각도. 중심에 너무 붙거나 기계에서
	// 너무 멀면 방향을 판단할 수 없어 false 다.
	bool GetCrankAngle(float& Angle) const;

	UPROPERTY(Transient) TObjectPtr<UUEGachaStudioWidget> Widget;
	UPROPERTY(Transient) TArray<TObjectPtr<AUEGachaMachine>> Machines;

	// 우리가 띄운 것. 레벨에 놓인 기계는 여기 들어가지 않는다 — 남의 것을
	// 정리하면 안 되기 때문이다.
	UPROPERTY(Transient) TArray<TObjectPtr<AUEGachaMachine>> SpawnedMachines;
	UPROPERTY(Transient) TObjectPtr<AUEGachaMachine> CurrentMachine;

	// 닫을 때 되돌릴 것들. 뽑기방은 닫지 않으므로 쓰이지 않는다.
	UPROPERTY(Transient) TObjectPtr<AActor> PreviousViewTarget;
	bool bRestoreCursor = false;

	bool bOpen = false;
	bool bDragging = false;
	float PreviousAngle = 0;

	// 카메라가 기계로 옮겨가는 동안은 손잡이를 못 잡게 한다. 블렌드 중에
	// 잡으면 화면과 손잡이 위치가 어긋난 채로 각도가 계산된다.
	float CameraReadyTime = 0;
};
