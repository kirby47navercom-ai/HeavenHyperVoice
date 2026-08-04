// Fill out your copyright notice in the Description page of Project Settings.

#include "UELoginWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

UUELoginWidget::UUELoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TitleText = FText::FromString(TEXT("HEAVEN HYPER VOICE"));
	SubtitleText = FText::FromString(TEXT("Client Login"));
	IdHintText = FText::FromString(TEXT("ID"));
	PasswordHintText = FText::FromString(TEXT("Password"));
	LoginButtonText = FText::FromString(TEXT("LOGIN"));
	ReadyStatusText = FText::FromString(TEXT("Ready"));
	EmptyInputStatusText = FText::FromString(TEXT("ID and password are required."));
	SubmittedStatusText = FText::FromString(TEXT("Login request submitted."));
}

TSharedRef<SWidget> UUELoginWidget::RebuildWidget()
{
	// UMG Designer 트리를 Python으로 안정적으로 수정하기 어려워서 런타임 트리를 직접 만든다.
	// BP 자식은 이 클래스의 노출된 값만 바꿔도 같은 구조의 로그인 화면을 재사용할 수 있다.
	BuildLoginLayout();
	return Super::RebuildWidget();
}

void UUELoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton)
	{
		LoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoginClicked);
	}

	SetStatusMessage(ReadyStatusText);
}

void UUELoginWidget::NativeDestruct()
{
	if (LoginButton)
	{
		LoginButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoginClicked);
	}

	Super::NativeDestruct();
}

FString UUELoginWidget::GetUserId() const
{
	return IdInputBox ? IdInputBox->GetText().ToString() : FString();
}

FString UUELoginWidget::GetPassword() const
{
	return PasswordInputBox ? PasswordInputBox->GetText().ToString() : FString();
}

void UUELoginWidget::SetStatusMessage(const FText& Message)
{
	if (StatusBlock)
	{
		StatusBlock->SetText(Message);
	}
}

void UUELoginWidget::BuildLoginLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background_Fullscreen"));
	BackgroundBorder->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.024f, 1.0f));

	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(BackgroundBorder))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
		BackgroundSlot->SetZOrder(0);
	}

	TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	TitleBlock->SetText(TitleText);
	TitleBlock->SetFontSize(38);
	TitleBlock->SetJustification(ETextJustify::Center);
	AddCenteredCanvasSlot(TitleBlock, FVector2D(0.0f, TitleOffsetY), FVector2D(760.0f, 56.0f), 2);

	SubtitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Subtitle"));
	SubtitleBlock->SetText(SubtitleText);
	SubtitleBlock->SetFontSize(18);
	SubtitleBlock->SetJustification(ETextJustify::Center);
	AddCenteredCanvasSlot(SubtitleBlock, FVector2D(0.0f, SubtitleOffsetY), FVector2D(PanelWidth, 34.0f), 2);

	IdInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_Id"));
	IdInputBox->SetHintText(IdHintText);
	IdInputBox->SetForegroundColor(InputTextColor);
	AddCenteredCanvasSlot(IdInputBox, FVector2D(0.0f, IdInputOffsetY), FVector2D(PanelWidth, InputHeight), 2);

	PasswordInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_Password"));
	PasswordInputBox->SetHintText(PasswordHintText);
	PasswordInputBox->SetForegroundColor(InputTextColor);
	PasswordInputBox->SetIsPassword(true);
	AddCenteredCanvasSlot(PasswordInputBox, FVector2D(0.0f, PasswordInputOffsetY), FVector2D(PanelWidth, InputHeight), 2);

	LoginButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Login"));
	AddCenteredCanvasSlot(LoginButton, FVector2D(0.0f, LoginButtonOffsetY), FVector2D(PanelWidth, ButtonHeight), 2);

	LoginButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_LoginButton"));
	LoginButtonLabel->SetText(LoginButtonText);
	LoginButtonLabel->SetFontSize(20);
	LoginButtonLabel->SetJustification(ETextJustify::Center);
	LoginButton->AddChild(LoginButtonLabel);

	StatusBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	StatusBlock->SetText(ReadyStatusText);
	StatusBlock->SetFontSize(15);
	StatusBlock->SetJustification(ETextJustify::Center);
	AddCenteredCanvasSlot(StatusBlock, FVector2D(0.0f, StatusOffsetY), FVector2D(PanelWidth, 32.0f), 2);
}

UCanvasPanelSlot* UUELoginWidget::AddCenteredCanvasSlot(UWidget* Widget, const FVector2D& Position, const FVector2D& Size, int32 ZOrder) const
{
	if (!RootCanvas || !Widget)
	{
		return nullptr;
	}

	UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Widget);
	if (!CanvasSlot)
	{
		return nullptr;
	}

	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
	CanvasSlot->SetZOrder(ZOrder);
	return CanvasSlot;
}

void UUELoginWidget::HandleLoginClicked()
{
	const FString UserId = GetUserId().TrimStartAndEnd();
	const FString Password = GetPassword();

	if (UserId.IsEmpty() || Password.IsEmpty())
	{
		SetStatusMessage(EmptyInputStatusText);
		return;
	}

	// 여기서 서버로 바로 보내지 않는다.
	// 서버 프로토콜이 바뀌어도 이 화면은 유지되도록, 제출 이벤트만 밖으로 열어둔다.
	SetStatusMessage(SubmittedStatusText);
	OnLoginSubmitted.Broadcast(UserId, Password);
}
