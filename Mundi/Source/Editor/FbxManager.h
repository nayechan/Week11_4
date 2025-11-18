#pragma once
#include "String.h"
#include "UEContainer.h"

struct FSkeleton;
struct FSkeletalMesh;

/**
 * FFbxManager
 * FBX 애셋의 메모리 레벨 캐싱 관리
 * 패턴: FObjManager (정적 캐시 클래스)
 */
class FFbxManager
{
private:
	static TMap<FString, FSkeleton*> SkeletonMap; // FilePath → Skeleton
	static TMap<FString, FSkeleton*> SkeletonNameMap; // Name → Skeleton
	static TMap<FString, FSkeletalMesh*> SkeletalMeshMap;

public:
	/**
	 * GetSkeleton - 캐시된 Skeleton 조회 (FilePath 기반)
	 * @param FilePath - FBX 파일 경로
	 * @return FSkeleton* 또는 nullptr
	 */
	static FSkeleton* GetSkeleton(const FString& FilePath);

	/**
	 * GetSkeletonByName - 캐시된 Skeleton 조회 (Name 기반)
	 * @param SkeletonName - Skeleton 이름
	 * @return FSkeleton* 또는 nullptr
	 */
	static FSkeleton* GetSkeletonByName(const FString& SkeletonName);

	/**
	 * RegisterSkeleton - Skeleton을 캐시에 등록
	 * @param FilePath - FBX 파일 경로
	 * @param Skeleton - 등록할 Skeleton 포인터
	 */
	static void RegisterSkeleton(const FString& FilePath, FSkeleton* Skeleton);

	/**
	 * GetAllSkeletonNames - 모든 Skeleton 이름 반환 (UI용)
	 * @return Skeleton 이름 목록
	 */
	static TArray<FString> GetAllSkeletonNames();

	/**
	 * LoadSkeletalMeshAsset - SkeletalMesh 로드 (캐시 우선)
	 * @param FilePath - FBX 파일 경로
	 * @return FSkeletalMesh* 또는 nullptr
	 */
	static FSkeletalMesh* LoadSkeletalMeshAsset(const FString& FilePath);

	/**
	 * Clear - 모든 캐시 정리
	 */
	static void Clear();

	/**
	 * Preload - 사전 로딩
	 */
	static void Preload();
};
