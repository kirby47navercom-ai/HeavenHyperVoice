// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreCollisionExport.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

class FHeavenHyperVoiceEditorModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		    this, &FHeavenHyperVoiceEditorModule::RegisterMenus));
	}

	void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		auto* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		auto& Section = Menu->FindOrAddSection("HHV", FText::FromString(TEXT("하이퍼보이스")));
		Section.AddMenuEntry(
		    "SaveSharedMovementCollision", FText::FromString(TEXT("공통 이동 충돌 저장")),
		    FText::FromString(TEXT(
		        "맵을 저장하고 ServerGround / ServerWall 충돌 파일을 클라이언트와 서버 폴더에 함께 저장합니다.")),
		    FSlateIcon(),
		    FUIAction(FExecuteAction::CreateLambda(
		                  []
		                  {
			                  UHHVCoreCollisionExportLibrary::ExportCurrentMapCollision();
		                  }),
		              FCanExecuteAction::CreateLambda(
		                  []
		                  {
			                  return GEditor && !GEditor->PlayWorld;
		                  })));
	}
};

IMPLEMENT_MODULE(FHeavenHyperVoiceEditorModule, HeavenHyperVoiceEditor)
