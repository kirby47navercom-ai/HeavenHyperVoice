// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HeavenHyperVoice : ModuleRules
{
	public HeavenHyperVoice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// YANG2_CLIENT_AUTHORITY_ONLY
		// Yang2는 서버 없는 단독 시험 브랜치다. 이 정의가 main에 들어가면
		// 클라이언트가 서버 권위를 우회하므로 절대 병합하지 않는다.
		PublicDefinitions.Add("HHV_YANG2_CLIENT_AUTHORITY_ONLY=1");
		// YANG2_CLIENT_AUTHORITY_ONLY: 서버와 같은 Lua BT/길찾기 소스를 재사용해요.
		bEnableExceptions = true;
		string Repo = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../.."));
		string Native = Path.Combine(Repo, "Server/build/vs2022/vcpkg_installed/x64-windows");
		if (!Directory.Exists(Native)) throw new BuildException("Build the server vcpkg dependencies before Yang2 offline AI.");
		PrivateIncludePaths.AddRange(new string[] {
			Path.Combine(Repo,"Server/InstanceServer/src"), Path.Combine(Repo,"Server/Movement/src"),
			Path.Combine(Repo,"Server/Protocol"), Path.Combine(Repo,"Server/FieldShared/src"),
			Path.Combine(Native,"include/recastnavigation") });
		// 서버 OpenSSL 헤더가 언리얼 OpenSSL을 가리지 않도록 AI 의존 헤더만 모아요.
		string AiHeaders=Path.Combine(Repo,"Client/Intermediate/Yang2AI/include");
		Directory.CreateDirectory(AiHeaders);
		foreach(string Folder in new string[] { "sol", "spdlog", "fmt" })
			foreach(string Header in Directory.GetFiles(Path.Combine(Native,"include",Folder),"*",SearchOption.AllDirectories)) {
				string Dest=Path.Combine(AiHeaders,Path.GetRelativePath(Path.Combine(Native,"include"),Header));
				Directory.CreateDirectory(Path.GetDirectoryName(Dest));
				if(!File.Exists(Dest) || File.GetLastWriteTimeUtc(Dest)!=File.GetLastWriteTimeUtc(Header)) File.Copy(Header,Dest,true);
			}
		foreach(string Header in new string[] { "lua.h", "lua.hpp", "luaconf.h", "lualib.h", "lauxlib.h" })
			File.Copy(Path.Combine(Native,"include",Header),Path.Combine(AiHeaders,Header),true);
		PublicSystemIncludePaths.Add(AiHeaders);
		PrivateDefinitions.AddRange(new string[] { "SPDLOG_FMT_EXTERNAL", "FMT_HEADER_ONLY" });
		foreach (string Lib in new string[] { "lua", "Recast", "Detour" })
			PublicAdditionalLibraries.Add(Path.Combine(Native,"lib",Lib+".lib"));
		RuntimeDependencies.Add("$(BinaryOutputDir)/lua.dll",Path.Combine(Native,"bin/lua.dll"));
		PublicDelayLoadDLLs.Add("lua.dll");
		// 배포 폴더에서도 같은 Lua 파일을 사용해요. 스크립트 위치는 브릿지 BP/Config에서 지정해요.
		string AiScripts=Path.Combine(Repo,"Server/InstanceServer/scripts");
		foreach(string File in Directory.GetFiles(AiScripts,"*.lua",SearchOption.AllDirectories))
			RuntimeDependencies.Add("$(BinaryOutputDir)/Yang2AI/"+Path.GetRelativePath(AiScripts,File),File,StagedFileType.NonUFS);

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../MovementCore")));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Net/Generated"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[] { "Shell32.lib", "Ole32.lib" });
		}
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "Water",
			"AnimGraphRuntime",
			"InputCore",
			"EnhancedInput",
			"AssetRegistry",
			"GameplayTags",
			"UMG",
			"GameplayAbilities",
			"GameplayTasks","GeometryCore",
			"GeometryFramework","ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"MoviePlayer",
			"DeveloperSettings",
			"Niagara",
			"Landscape",

			// Field server transport. The engine ships OpenSSL 1.1.1t, which is
			// enough for TLS 1.3 with X25519 -- the field server sets no group or
			// cipher restrictions, so there is nothing to match on our side.
			"OpenSSL"
		});

		// FlatBuffers runtime headers, vendored. The engine only ships the
		// licence notice for FlatBuffers, not the headers, so there is nothing
		// to depend on instead. See ThirdParty/FlatBuffers/README.md.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "FlatBuffers"));
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
