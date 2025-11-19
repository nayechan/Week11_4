#include "pch.h"
#include "AnimationAsset.h"
#include "GlobalConsole.h"
#include "FbxManager.h"
#include "JsonSerializer.h"
#include "PathUtils.h"
#include <filesystem>

void UAnimationAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// Skeleton 직렬화는 .anim.json 오버라이드 파일에서 처리됨
	// (SaveOverrideData/LoadOverrideData 사용)
}

bool UAnimationAsset::SaveOverrideData(const FString& FilePath)
{
	// 기존 .skeleton 파일 로드 (있다면)
	JSON RootJson = JSON::Make(JSON::Class::Object);
	FWideString WidePath = UTF8ToWide(FilePath);

	// 기존 파일이 있으면 로드하여 다른 섹션 보존
	if (std::filesystem::exists(UTF8ToWide(FilePath)))
	{
		FJsonSerializer::LoadJsonFromFile(RootJson, WidePath);
	}

	// "Animation" 섹션 생성/업데이트
	JSON AnimSection = JSON::Make(JSON::Class::Object);
	if (Skeleton && !Skeleton->Name.empty())
	{
		AnimSection["SkeletonName"] = Skeleton->Name;
	}
	RootJson["Animation"] = AnimSection;

	// JSON 파일로 저장
	return FJsonSerializer::SaveJsonToFile(RootJson, WidePath);
}

bool UAnimationAsset::LoadOverrideData(const FString& FilePath)
{
	// JSON 파일 로드
	JSON RootJson;
	FWideString WidePath = UTF8ToWide(FilePath);
	if (!FJsonSerializer::LoadJsonFromFile(RootJson, WidePath))
	{
		return false;
	}

	// "Animation" 섹션에서 Skeleton 오버라이드 복원
	JSON AnimSection;
	if (FJsonSerializer::ReadObject(RootJson, "Animation", AnimSection, nullptr, false))
	{
		FString SkeletonName;
		if (FJsonSerializer::ReadString(AnimSection, "SkeletonName", SkeletonName, "", false) && !SkeletonName.empty())
		{
			Skeleton = FFbxManager::GetSkeletonByName(SkeletonName);
			if (Skeleton)
			{
				SkeletonID = Skeleton->Name;
				UE_LOG("Loaded Animation skeleton override: %s", SkeletonName.c_str());
			}
			else
			{
				UE_LOG("Warning: Could not find skeleton '%s' in FFbxManager", SkeletonName.c_str());
			}
		}
	}

	return true;
}
