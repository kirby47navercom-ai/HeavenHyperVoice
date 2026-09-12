#include "CoreCollisionExport.h"
#include "../../HeavenHyperVoice/Movement/UECoreCollisionSubsystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
void NotifyExport(const FString& Message, bool bSuccess)
{
	UE_LOG(LogTemp, Display, TEXT("Shared collision export: %s"), *Message);

	if (!IsRunningCommandlet())
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = bSuccess ? 7.f : 12.f;
		const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);

		if (Notification.IsValid())
			Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success
			                                          : SNotificationItem::CS_Fail);
	}
}

// Prepare both outputs before replacing either. Roll back the first if the second cannot be installed.
bool SavePair(const UUECoreCollisionSubsystem& Snapshot, const FString& Client, const FString& Server)
{
	auto& Files = IFileManager::Get();
	const FString Suffix = TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Paths[2] = {Client, Server};
	FString Staged[2], Backups[2];
	bool Existed[2] = {false, false};
	auto Cleanup = [&]()
	{
		for (int I = 0; I < 2; ++I)
		{
			Files.Delete(*Staged[I], false, false, true);
			Files.Delete(*Backups[I], false, false, true);
		}
	};

	for (int I = 0; I < 2; ++I)
	{
		Staged[I] = Paths[I] + Suffix + TEXT(".tmp");
		Backups[I] = Paths[I] + Suffix + TEXT(".old");
	}

	for (int I = 0; I < 2; ++I)
	{
		Files.MakeDirectory(*FPaths::GetPath(Paths[I]), true);
		Existed[I] = Files.FileExists(*Paths[I]);

		if ((Existed[I] && (Files.IsReadOnly(*Paths[I]) ||
		                    Files.Copy(*Backups[I], *Paths[I], false, false, false) != COPY_OK)) ||
		    !Snapshot.SaveCollisionFile(Staged[I]))
		{
			Cleanup();
			return false;
		}
	}

	for (int I = 0; I < 2; ++I)
	{
		if (!Files.Move(*Paths[I], *Staged[I], true, false, false, true))
		{
			bool bRestored = true;

			for (int J = 0; J <= I; ++J)
			{
				if (Existed[J])
					bRestored =
					    (Files.Copy(*Paths[J], *Backups[J], true, false, false) == COPY_OK) && bRestored;
				else
					bRestored = Files.Delete(*Paths[J], false, false, true) && bRestored;
			}
			// Preserve backups for manual recovery if the filesystem also rejects rollback.
			if (bRestored)
				Cleanup();
			else
				UE_LOG(LogTemp, Error, TEXT("Collision export rollback failed. Recovery files: %s, %s"),
				       *Backups[0], *Backups[1]);
			return false;
		}
	}
	Cleanup();
	return true;
}
} // namespace

bool UHHVCoreCollisionExportLibrary::ExportCurrentMapCollision(bool bSaveMap)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		NotifyExport(TEXT("플레이를 종료한 뒤 공통 이동 충돌을 저장해 주세요."), false);
		return false;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();

	if (!World || (bSaveMap && !FEditorFileUtils::SaveDirtyPackages(false, true, false)))
		return false;
	auto* Snapshot = World->GetSubsystem<UUECoreCollisionSubsystem>();

	if (!Snapshot || Snapshot->GetDefaultCollisionFile().IsEmpty())
	{
		NotifyExport(TEXT("먼저 맵을 프로젝트 Content 폴더에 저장해 주세요."), false);
		return false;
	}

	if (!Snapshot->RebuildFromScene())
	{
		NotifyExport(
		    TEXT("충돌 추출 실패: ServerGround / ServerWall 프리셋과 Output Log를 확인해 주세요. 기존 파일은 "
		         "유지됩니다."),
		    false);
		return false;
	}
	const FString Client = Snapshot->GetDefaultCollisionFile();
	FString Server = FPaths::ConvertRelativePathToFull(
	    FPaths::ProjectDir() / TEXT("../Server/maps/collision") / Snapshot->GetCollisionRelativePath());
	FPaths::CollapseRelativeDirectories(Server);

	if (!SavePair(*Snapshot, Client, Server))
	{
		NotifyExport(
		    TEXT("충돌 파일 저장 실패: 클라이언트와 서버 폴더의 쓰기 권한 및 Output Log를 확인해 주세요."),
		    false);
		return false;
	}

	if (!Snapshot->LoadCollisionFile(Client))
		return false;
	NotifyExport(FString::Printf(TEXT("공통 이동 충돌 저장 완료 · 삼각형 %d개\n클라이언트: %s\n서버: %s"),
	                             Snapshot->TriangleCount, *Client, *Server),
	             true);
	return true;
}
