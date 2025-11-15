#pragma once
#include "fbxsdk.h"
#include "String.h"
#include "UEContainer.h"
#include "FbxScene.h"
#include "FbxMesh.h"
#include "FbxAnimation.h"
#include "FbxMaterial.h"

// Forward declarations
class USkeletalMesh;
class UAnimSequence;
struct FSkeleton;
struct FSkeletalMeshData;
struct FMaterialInfo;

/**
 * FFbxParser
 *
 * FBX SDK 관리 및 로딩 orchestration을 담당하는 파서 클래스
 *
 * UE5 Pattern: InterchangeFbxParser (단순화 버전)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxAPI.h
 *
 * 주요 책임:
 * - FbxManager/FbxImporter/FbxScene 생명주기 관리
 * - FbxScene* 메모리 캐싱 (메시+애니메이션 로드 간 공유)
 * - 좌표계 변환 (ConvertScene)
 * - 각 모듈 조율 (FFbxScene, FFbxMesh, FFbxAnimation, FFbxMaterial)
 *
 * 설계 결정:
 * - Immediate loading 패턴 (payload context 없음)
 * - 직접 구현 (인터페이스 없음)
 * - SDK 레벨 캐싱만 담당 (.bin/.anim.bin 캐싱은 UFbxLoader에서)
 */
class FFbxParser
{
public:
	FFbxParser();
	~FFbxParser();

	// ========================================
	// Public API - Immediate Loading
	// ========================================

	/**
	 * LoadFbxMesh
	 *
	 * FBX 파일에서 스켈레탈 메시를 즉시 로드하여 반환
	 *
	 * @param FilePath - 로드할 FBX 파일 경로
	 * @param OutMaterialInfos - 머티리얼 정보 출력 (bin 캐시 저장용)
	 * @return 로드된 스켈레탈 메시 데이터 (실패 시 nullptr)
	 */
	FSkeletalMeshData* LoadFbxMesh(const FString& FilePath, TArray<FMaterialInfo>& OutMaterialInfos);

	/**
	 * LoadFbxAnimation
	 *
	 * FBX 파일에서 애니메이션을 즉시 로드하여 반환
	 *
	 * @param FilePath - 로드할 FBX 파일 경로
	 * @param TargetSkeleton - 타겟 스켈레톤 (본 매칭용)
	 * @return 로드된 애니메이션 시퀀스 (실패 시 nullptr)
	 */
	UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton);

	/**
	 * Reset
	 *
	 * 캐시된 FbxScene 정리
	 */
	void Reset();

	// ========================================
	// Public Member Variable (UE5 Pattern)
	// ========================================

	/**
	 * JointOrientationMatrix
	 *
	 * IMPORTANT: 이것은 함수가 아니라 public 멤버 변수입니다!
	 *
	 * UE5 Pattern: FFbxParser::JointPostConversionMatrix (public 멤버)
	 * Location: FbxAPI.h:156
	 *
	 * ConvertScene()에서 bForceFrontXAxis에 따라 설정됨:
	 * - bForceFrontXAxis = false (+Y Forward, UE5 기본값): Identity Matrix
	 * - bForceFrontXAxis = true  (+X Forward): 90도 회전 행렬
	 *
	 * FFbxMesh::ExtractMeshData()에서 BindPose 계산 시 사용:
	 * GlobalBindPoseMatrix = TransformLinkMatrix * JointOrientationMatrix
	 */
	FbxAMatrix JointOrientationMatrix;

private:
	// ========================================
	// SDK 관리
	// ========================================

	FbxManager* SDKManager;
	FbxImporter* SDKImporter;

	// ========================================
	// Scene 캐싱 구조
	// ========================================

	/**
	 * FCachedFbxScene
	 *
	 * FbxScene* 메모리 캐싱을 위한 구조체
	 *
	 * 목적: 같은 FBX 파일에서 메시와 애니메이션을 모두 로드할 때
	 *       FbxScene을 재사용하여 파싱 비용 절감
	 *
	 * 생명주기:
	 * - 다른 파일 로드 시 자동 해제
	 * - Reset() 호출 시 수동 해제
	 */
	struct FCachedFbxScene
	{
		FbxScene* Scene = nullptr;
		FString FilePath;
		bool bConverted = false;
		bool bForceFrontXAxis = false;  // UE5 Pattern: false = -Y Forward (default), true = +X Forward

		bool IsValid(const FString& InFilePath) const
		{
			return Scene != nullptr && FilePath == InFilePath;
		}
	};

	FCachedFbxScene CachedScene;

	// ========================================
	// 각 모듈 인스턴스
	// ========================================

	FFbxScene SceneProcessor;
	FFbxMesh MeshProcessor;
	FFbxAnimation AnimationProcessor;
	FFbxMaterial MaterialProcessor;

	// ========================================
	// Private Methods
	// ========================================

	/**
	 * LoadFbxScene
	 *
	 * FBX 파일을 로드하고 좌표계 변환을 수행
	 *
	 * UE5 Pattern: FFbxParser::LoadFbxFile
	 * Location: FbxAPI.cpp:250-350
	 *
	 * 수행 작업:
	 * 1. FbxImporter로 FBX 파일 읽기
	 * 2. 좌표계 변환 (ConvertScene)
	 * 3. 단위 변환 (meter)
	 * 4. 삼각화 (Triangulate)
	 * 5. FbxScene* 캐싱
	 *
	 * @param FilePath - 로드할 FBX 파일 경로
	 * @param bOutNewlyLoaded - 새로 로드되었는지 여부 (true: 새 로드, false: 캐시)
	 * @return 로드된 FbxScene* (실패 시 nullptr)
	 */
	FbxScene* LoadFbxScene(const FString& FilePath, bool& bOutNewlyLoaded);

	/**
	 * ConvertScene
	 *
	 * FbxScene의 좌표계를 UE 좌표계로 변환
	 *
	 * UE5 Pattern: FFbxParser::ConvertScene
	 * Location: FbxAPI.cpp:450-520
	 *
	 * 좌표계 변환:
	 * - Source: FBX 파일의 원본 좌표계 (다양함)
	 * - Intermediate: Z-Up, Right-Handed (-Y Forward 또는 +X Forward)
	 * - Final: Z-Up, Left-Handed (Y-Flip으로 변환, FFbxConvert에서 수행)
	 *
	 * JointOrientationMatrix 설정:
	 * - bForceFrontXAxis = false: Identity (기본값)
	 * - bForceFrontXAxis = true:  90도 회전 행렬
	 *
	 * @param Scene - 변환할 FbxScene
	 * @param bForceFrontXAxis - +X Forward 강제 사용 여부 (기본: false)
	 */
	void ConvertScene(FbxScene* Scene, bool bForceFrontXAxis);

	// Copy/move 방지
	FFbxParser(const FFbxParser&) = delete;
	FFbxParser& operator=(const FFbxParser&) = delete;
};
