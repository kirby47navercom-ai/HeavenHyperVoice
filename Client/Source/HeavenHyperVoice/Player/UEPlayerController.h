// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UEPlayerController.generated.h"

class UUELoginWidget;

/**
 * 클라이언트 첫 화면을 관리하는 PlayerController다.
 *
 * 서버 접속 전에는 캐릭터 조작보다 UI 입력이 우선이므로 BeginPlay에서 로그인 위젯을 띄운다.
 * LoginWidgetClass를 BP 자식으로 바꾸면 화면 문구와 수치를 디자이너에서 바로 조절할 수 있다.
 */
UCLASS()
class HEAVENHYPERVOICE_API AUEPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUEPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Login")
	void ShowLoginScreen();

	UFUNCTION(BlueprintCallable, Category = "Login")
	void HideLoginScreen();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Login")
	TSubclassOf<UUELoginWidget> LoginWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	bool bShowLoginOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login")
	int32 LoginWidgetZOrder = 100;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUELoginWidget> LoginWidgetInstance = nullptr;
};
