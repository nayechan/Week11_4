#pragma once
#include "Object.h"

// Forward declarations
class UAnimSequence;
struct FBoneAnimationTrack;
struct FSkeleton;
class FFbxParser;

/**
 * UFbxLoader
 *
 * FBX 로딩을 위한 얇은 래퍼 클래스 (애셋 레벨 캐싱 담당)
 *
 * Phase 7: Orchestration 로직을 FFbxParser로 이동
 *
 * 주요 책임:
 * - .bin 파일 캐싱 (메시)
 * - .anim.bin 파일 캐싱 (애니메이션)
 * - 타임스탬프 기반 캐시 유효성 검증
 * - FFbxParser로 작업 위임
 *
 * 책임 분리:
 * - UFbxLoader: 애셋 직렬화 캐싱 (Layer 1)
 * - FFbxParser: FBX SDK 관리 및 orchestration (Layer 2)
 */
class UFbxLoader : public UObject
{
public:
	DECLARE_CLASS(UFbxLoader, UObject)

	static UFbxLoader& GetInstance();
	UFbxLoader();

	static void PreLoad();

	/**
	 * LoadFbxMesh
	 *
	 * FBX 파일에서 스켈레탈 메시를 로드
	 * 기존 리소스 확인 후 없으면 LoadFbxMeshAsset() 호출
	 *
	 * @param FilePath - FBX 파일 경로
	 * @return 로드된 USkeletalMesh (실패 시 nullptr)
	 */
	USkeletalMesh* LoadFbxMesh(const FString& FilePath);

	/**
	 * LoadFbxMeshAsset
	 *
	 * FBX 파일에서 메시 데이터를 로드 (애셋 캐싱 처리)
	 *
	 * 흐름:
	 * 1. .bin 캐시 확인 (타임스탬프 검증)
	 * 2. 캐시 유효 → 로드 후 반환
	 * 3. 캐시 무효 → FFbxParser::LoadFbxMesh() 호출 → 캐시 저장
	 *
	 * @param FilePath - FBX 파일 경로
	 * @return 로드된 FSkeletalMeshData (실패 시 nullptr)
	 */
	FSkeletalMeshData* LoadFbxMeshAsset(const FString& FilePath);

	/**
	 * LoadFbxAnimation
	 *
	 * FBX 파일에서 애니메이션을 로드 (애셋 캐싱 처리)
	 *
	 * 흐름:
	 * 1. 기존 리소스 확인
	 * 2. .anim.bin 캐시 확인 (타임스탬프 검증)
	 * 3. 캐시 유효 → 로드 후 반환
	 * 4. 캐시 무효 → FFbxParser::LoadFbxAnimation() 호출 → 캐시 저장
	 *
	 * @param FilePath - FBX 파일 경로
	 * @param TargetSkeleton - 타겟 스켈레톤 (본 매칭용)
	 * @return 로드된 UAnimSequence (실패 시 nullptr)
	 */
	UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton);

protected:
	~UFbxLoader() override;

private:
	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;

	// ========================================
	// Phase 7: FFbxParser 위임
	// ========================================

	/**
	 * Parser
	 *
	 * FBX SDK 관리 및 orchestration을 담당하는 파서
	 * UFbxLoader는 애셋 캐싱만 처리하고 실제 파싱은 Parser에게 위임
	 */
	FFbxParser* Parser;

	// ========================================
	// 애셋 캐싱 (bin 파일 저장용)
	// ========================================

	/**
	 * MaterialInfos
	 *
	 * .bin 캐시 저장 시 함께 저장할 머티리얼 정보
	 * FFbxParser::LoadFbxMesh()에서 채워짐
	 */
	TArray<FMaterialInfo> MaterialInfos;
};