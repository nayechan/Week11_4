#include "pch.h"
#include "FbxManager.h"
#include "FbxLoader.h"
#include "PathUtils.h"
#include "VertexData.h"
#include "GlobalConsole.h"

TMap<FString, FSkeleton*> FFbxManager::SkeletonMap;
TMap<FString, FSkeleton*> FFbxManager::SkeletonNameMap;
TMap<FString, FSkeletalMesh*> FFbxManager::SkeletalMeshMap;

FSkeleton* FFbxManager::GetSkeleton(const FString& FilePath)
{
	FString NormalizedPath = NormalizePath(FilePath);

	if (FSkeleton** It = SkeletonMap.Find(NormalizedPath))
	{
		return *It;
	}

	return nullptr;
}

FSkeleton* FFbxManager::GetSkeletonByName(const FString& SkeletonName)
{
	if (FSkeleton** It = SkeletonNameMap.Find(SkeletonName))
	{
		return *It;
	}

	return nullptr;
}

void FFbxManager::RegisterSkeleton(const FString& FilePath, FSkeleton* Skeleton)
{
	if (!Skeleton) return;

	FString NormalizedPath = NormalizePath(FilePath);

	if (SkeletonMap.Find(NormalizedPath) == nullptr)
	{
		SkeletonMap.Add(NormalizedPath, Skeleton);

		// Name으로도 등록 (UI 조회용)
		if (!Skeleton->Name.empty())
		{
			SkeletonNameMap.Add(Skeleton->Name, Skeleton);
		}

		UE_LOG("FFbxManager: Registered Skeleton: %s", Skeleton->Name.c_str());
	}
}

TArray<FString> FFbxManager::GetAllSkeletonNames()
{
	TArray<FString> Names;
	for (auto& Pair : SkeletonMap)
	{
		if (Pair.second && !Pair.second->Name.empty())
		{
			Names.Add(Pair.second->Name);
		}
	}
	return Names;
}

FSkeletalMesh* FFbxManager::LoadSkeletalMeshAsset(const FString& FilePath)
{
	FString NormalizedPath = NormalizePath(FilePath);

	// 1. 메모리 캐시 확인
	if (FSkeletalMesh** It = SkeletalMeshMap.Find(NormalizedPath))
	{
		UE_LOG("FFbxManager: Cache hit for: %s", NormalizedPath.c_str());
		return *It;
	}

	// 2. UFbxLoader를 통해 로드 (디스크 캐시 + 파싱)
	FSkeletalMesh* MeshData = UFbxLoader::GetInstance().LoadFbxMeshAsset(NormalizedPath);

	if (!MeshData)
	{
		return nullptr;
	}

	// 3. 메모리 캐시에 등록
	SkeletalMeshMap.Add(NormalizedPath, MeshData);

	if (MeshData->Skeleton)
	{
		SkeletonMap.Add(NormalizedPath, MeshData->Skeleton);
	}

	UE_LOG("FFbxManager: Loaded and cached: %s", NormalizedPath.c_str());

	return MeshData;
}

void FFbxManager::Clear()
{
	// 1. SkeletalMesh 삭제 (Skeleton은 삭제하지 않음)
	for (auto& Pair : SkeletalMeshMap)
	{
		delete Pair.second;
	}

	// 2. Skeleton 명시적 삭제 (FFbxManager가 소유)
	for (auto& Pair : SkeletonMap)
	{
		delete Pair.second;
	}

	SkeletalMeshMap.Empty();
	SkeletonMap.Empty();
	SkeletonNameMap.Empty();  // Name 맵도 정리

	UE_LOG("FFbxManager: Cleared all cached assets");
}

void FFbxManager::Preload()
{
	// UFbxLoader의 PreLoad 재사용
	UFbxLoader::PreLoad();
}
