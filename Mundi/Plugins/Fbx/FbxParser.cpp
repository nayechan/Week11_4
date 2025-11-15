#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // Disable double to float conversion warning for FBX SDK
#include "FbxParser.h"
#include "FbxConvert.h"
#include "FbxHelper.h"
#include "fbxsdk/fileio/fbxiosettings.h"
#include "fbxsdk/scene/geometry/fbxcluster.h"
#include "SkeletalMesh.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"

// ========================================
// 생성자/소멸자
// ========================================

FFbxParser::FFbxParser()
	: SDKManager(nullptr)
	, SDKImporter(nullptr)
{
	// FbxManager 생성 (메모리 관리, FbxManager 소멸 시 Fbx 관련 오브젝트 모두 소멸)
	SDKManager = FbxManager::Create();

	// JointOrientationMatrix 초기화 (Identity)
	JointOrientationMatrix.SetIdentity();
}

FFbxParser::~FFbxParser()
{
	// Scene 정리
	Reset();

	// FbxManager 소멸
	if (SDKManager)
	{
		SDKManager->Destroy();
		SDKManager = nullptr;
	}
}

// ========================================
// Public API
// ========================================

FSkeletalMeshData* FFbxParser::LoadFbxMesh(const FString& FilePath, TArray<FMaterialInfo>& OutMaterialInfos)
{
	OutMaterialInfos.clear();

	// 1. FbxScene 로드 (캐시 활용)
	bool bNewlyLoaded = false;
	UE_LOG("  [FFbxParser] Calling LoadFbxScene...");
	FbxScene* Scene = LoadFbxScene(FilePath, bNewlyLoaded);
	if (!Scene)
	{
		UE_LOG("Error: FFbxParser::LoadFbxMesh - Failed to load FBX scene: %s", FilePath.c_str());
		return nullptr;
	}
	UE_LOG("  [FFbxParser] LoadFbxScene returned: %s", bNewlyLoaded ? "NEW SCENE" : "CACHED SCENE");

	UE_LOG("========================================");
	UE_LOG("[MESH] FFbxParser: Loading Skeletal Mesh from FBX: %s", FilePath.c_str());
	UE_LOG("========================================");

	// 2. 메시 데이터 생성
	FSkeletalMeshData* MeshData = new FSkeletalMeshData();
	MeshData->PathFileName = FilePath;

	// 3. 루트 노드 획득
	FbxNode* RootNode = Scene->GetRootNode();
	if (!RootNode)
	{
		UE_LOG("Error: FFbxParser::LoadFbxMesh - No root node in scene");
		delete MeshData;
		return nullptr;
	}

	// 4. 인덱싱 맵 준비
	// 뼈의 인덱스를 부여 (FBX는 정점이 아니라 뼈 중심으로 데이터 저장)
	TMap<FbxNode*, int32> BoneToIndex;

	// 머티리얼 인덱스 생성 (FBX는 머티리얼 순서가 뒤죽박죽)
	TMap<FbxSurfaceMaterial*, int32> MaterialToIndex;

	// 머티리얼별 인덱스 리스트 (렌더링 비용 절감을 위한 정렬)
	TMap<int32, TArray<uint32>> MaterialGroupIndexList;

	// 머티리얼이 없는 노드를 위한 기본 그룹 (0번)
	MaterialGroupIndexList.Add(0, TArray<uint32>());
	MaterialToIndex.Add(nullptr, 0);
	MeshData->GroupInfos.Add(FGroupInfo());

	// 5. 스켈레톤 추출 (Phase 5: FFbxScene)
	// Blender FBX는 "Armature" dummy node를 스킵하도록 bCreatorIsBlender 전달
	FFbxScene::ExtractSkeleton(Scene, RootNode, *MeshData, BoneToIndex, bCreatorIsBlender);

	// 6. 메시 추출 (Phase 6: FFbxMesh)
	for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
	{
		FFbxMesh::ExtractMeshFromNode(
			RootNode->GetChild(Index),
			*MeshData,
			MaterialGroupIndexList,
			BoneToIndex,
			MaterialToIndex,
			OutMaterialInfos,
			Scene,
			FilePath,
			JointOrientationMatrix  // UE5 Pattern: 멤버 변수 직접 사용
		);
	}

	// 7. 여러 루트 본이 있으면 가상 루트 생성
	FFbxScene::EnsureSingleRootBone(*MeshData);

	// 8. 머티리얼 플래그 설정
	if (MeshData->GroupInfos.Num() > 1)
	{
		MeshData->bHasMaterial = true;
	}

	// 9. 머티리얼 그룹별 인덱스 병합
	uint32 Count = 0;
	for (auto& Element : MaterialGroupIndexList)
	{
		int32 MaterialIndex = Element.first;
		const TArray<uint32>& IndexList = Element.second;

		// 최종 인덱스 배열에 추가
		MeshData->Indices.Append(IndexList);

		// GroupInfo에 StartIndex와 Count 설정
		MeshData->GroupInfos[MaterialIndex].StartIndex = Count;
		MeshData->GroupInfos[MaterialIndex].IndexCount = IndexList.Num();
		Count += IndexList.Num();
	}

	UE_LOG("========================================");
	UE_LOG("[MESH] FFbxParser: Load complete");
	UE_LOG("[MESH] Vertices: %d, Indices: %d, Bones: %d, Materials: %d",
		   MeshData->Vertices.Num(),
		   MeshData->Indices.Num(),
		   MeshData->Skeleton.Bones.Num(),
		   MeshData->GroupInfos.Num());
	UE_LOG("========================================");

	return MeshData;
}

UAnimSequence* FFbxParser::LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton)
{
	// 1. FbxScene 로드 (캐시 활용)
	bool bNewlyLoaded = false;
	FbxScene* Scene = LoadFbxScene(FilePath, bNewlyLoaded);
	if (!Scene)
	{
		UE_LOG("Error: FFbxParser::LoadFbxAnimation - Failed to load FBX scene: %s", FilePath.c_str());
		return nullptr;
	}

	// 2. AnimStack 확인
	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount == 0)
	{
		UE_LOG("Error: FFbxParser::LoadFbxAnimation - No animation data in FBX: %s", FilePath.c_str());
		return nullptr;
	}

	UE_LOG("Found %d animation stack(s) in FBX file", AnimStackCount);

	// 3. 모든 AnimStack을 검증하여 첫 번째 valid stack 찾기
	FbxAnimStack* ValidAnimStack = nullptr;
	int32 ValidAnimStackIndex = -1;

	for (int32 i = 0; i < AnimStackCount; i++)
	{
		FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(i);
		UE_LOG("  AnimStack[%d]: '%s'", i, AnimStack->GetName());

		// AnimStack이 애니메이션 데이터를 가지고 있는지 검증
		if (FFbxAnimation::HasAnimationData(AnimStack, TargetSkeleton, Scene))
		{
			UE_LOG("    -> HAS ANIMATION DATA - Using this stack");
			ValidAnimStack = AnimStack;
			ValidAnimStackIndex = i;
			break;  // 첫 번째 valid stack 사용
		}
		else
		{
			UE_LOG("    -> EMPTY (no animation curves) - Skipping");
		}
	}

	// 4. Valid AnimStack이 없으면 에러
	if (!ValidAnimStack)
	{
		UE_LOG("Error: All AnimStacks are empty (no animation curves found)");
		return nullptr;
	}

	UE_LOG("Selected AnimStack[%d]: '%s'", ValidAnimStackIndex, ValidAnimStack->GetName());

	// 5. UAnimSequence 생성
	UAnimSequence* AnimSeq = NewObject<UAnimSequence>();
	AnimSeq->SetFilePath(FilePath);
	AnimSeq->Skeleton = const_cast<FSkeleton*>(TargetSkeleton);

	// 6. 선택된 AnimStack 로드
	UE_LOG("========================================");
	UE_LOG("[ANIMATION] FFbxParser: Loading from FBX: %s", FilePath.c_str());
	UE_LOG("[ANIMATION] Animation stack name: %s", ValidAnimStack->GetName());
	UE_LOG("========================================");

	// 7. 애니메이션 추출 (Phase 4: FFbxAnimation)
	FFbxAnimation::ExtractAnimation(ValidAnimStack, TargetSkeleton, AnimSeq, JointOrientationMatrix);

	UE_LOG("========================================");
	UE_LOG("[ANIMATION] FFbxParser: Load complete");
	UE_LOG("[ANIMATION] Result: %d frames, %d bone tracks, %d total keys",
		   AnimSeq->NumberOfFrames,
		   AnimSeq->GetBoneAnimationTracks().Num(),
		   AnimSeq->NumberOfKeys);
	UE_LOG("========================================");

	return AnimSeq;
}

void FFbxParser::Reset()
{
	if (CachedScene.Scene)
	{
		CachedScene.Scene->Destroy();
		CachedScene.Scene = nullptr;
		CachedScene.FilePath.clear();
		CachedScene.bConverted = false;
		CachedScene.bForceFrontXAxis = false;
	}

	if (SDKImporter)
	{
		SDKImporter->Destroy();
		SDKImporter = nullptr;
	}
}

// ========================================
// Private Methods
// ========================================

FbxScene* FFbxParser::LoadFbxScene(const FString& FilePath, bool& bOutNewlyLoaded)
{
	bOutNewlyLoaded = false;

	// 1. 캐시 확인: 이미 같은 파일이 로드되어 있으면 캐시된 Scene 반환
	if (CachedScene.IsValid(FilePath))
	{
		return CachedScene.Scene;
	}

	// 2. 다른 파일이면 기존 Scene 정리
	if (CachedScene.Scene)
	{
		CachedScene.Scene->Destroy();
		CachedScene.Scene = nullptr;
		CachedScene.FilePath.clear();
		CachedScene.bConverted = false;
	}

	// 3. FbxImporter 생성 및 초기화
	SDKImporter = FbxImporter::Create(SDKManager, "");
	if (!SDKImporter->Initialize(FilePath.c_str(), -1, SDKManager->GetIOSettings()))
	{
		UE_LOG("Error: FbxImporter::Initialize() failed for: %s", FilePath.c_str());
		UE_LOG("Error: %s", SDKImporter->GetStatus().GetErrorString());
		SDKImporter->Destroy();
		SDKImporter = nullptr;
		return nullptr;
	}

	// 3.5. Blender FBX 감지 (UE5 Pattern: FbxAPI.cpp:181-187)
	// Example creator string: "Blender (stable FBX IO) - 2.78 (sub 0) - 3.7.7"
	// Blender FBX는 "Armature" dummy node를 root joint 부모로 포함하며,
	// 이 노드는 100x scale + 90° rotation을 가짐 (meter→cm + Z-up→Y-up 변환)
	bCreatorIsBlender = false;
	FbxIOFileHeaderInfo* FileHeaderInfo = SDKImporter->GetFileHeaderInfo();
	if (FileHeaderInfo && FileHeaderInfo->mCreator.Buffer())
	{
		FString CreatorString(FileHeaderInfo->mCreator.Buffer());
		// StartsWith 체크 (대소문자 구분)
		bCreatorIsBlender = (CreatorString.find("Blender") == 0);

		UE_LOG("[FFbxParser] FBX Creator: %s", CreatorString.c_str());
		UE_LOG("[FFbxParser] Blender FBX: %s", bCreatorIsBlender ? "YES" : "NO");
	}

	// 4. FbxScene 생성 및 Import
	FbxScene* Scene = FbxScene::Create(SDKManager, "Shared Scene");
	SDKImporter->Import(Scene);
	SDKImporter->Destroy();
	SDKImporter = nullptr;

	// 5. 좌표계 변환
	bool bForceFrontXAxis = true;  // UE5 default: false, Mundi: true
	ConvertScene(Scene, bForceFrontXAxis);

	// 6. 단위 변환 (meter로 변환)
	// UE5 Pattern: Always convert to ensure consistency regardless of source unit
	FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
	double ScaleFactor = SceneSystemUnit.GetScaleFactor();

	UE_LOG("[FFbxParser] FBX Unit System: scale factor = %.2f (1.0 = cm, 100.0 = m)", ScaleFactor);

	// ALWAYS convert to meters for consistency (Blender, Mixamo, Maya all standardized)
	FbxSystemUnit::m.ConvertScene(Scene);
	Scene->GetAnimationEvaluator()->Reset();  // Reset evaluator after unit conversion

	UE_LOG("[FFbxParser] FBX Unit converted to meters");

	// 7. 삼각화
	FbxGeometryConverter GeometryConverter(SDKManager);
	if (!GeometryConverter.Triangulate(Scene, true))
	{
		UE_LOG("[FFbxParser] Warning: Scene triangulation failed");
	}

	// 8. 캐시에 저장
	CachedScene.Scene = Scene;
	CachedScene.FilePath = FilePath;
	CachedScene.bConverted = true;
	CachedScene.bForceFrontXAxis = bForceFrontXAxis;
	bOutNewlyLoaded = true;

	return Scene;
}

void FFbxParser::ConvertScene(FbxScene* Scene, bool bForceFrontXAxis)
{
	// ========================================
	// UE5 Pattern: FFbxConvert::ConvertScene()로 위임
	// ========================================
	// UE5 Reference:
	// - FbxAPI.cpp Line 151: FFbxConvert::ConvertScene() 호출
	// - FbxConvert.cpp Line 113-158: 실제 구현
	//
	// 이 함수는 두 가지 작업을 수행:
	// 1. FBX Scene 좌표계를 Z-Up, Right-Handed로 변환
	// 2. JointOrientationMatrix 멤버 변수 설정 (참조로 전달)
	//
	// JointOrientationMatrix는 이후 모든 곳에서 재사용됨:
	// - FbxMesh.cpp: BindPose 계산 시 사용 (Parser.JointOrientationMatrix)
	// - FbxAnimation.cpp: 애니메이션 노드 변환 시 사용 (Parser.JointOrientationMatrix)
	// - FbxScene.cpp: 스켈레톤 계층 구조 변환 시 사용 (Parser.JointOrientationMatrix)

	FFbxConvert::ConvertScene(Scene, bForceFrontXAxis, JointOrientationMatrix);
}

#pragma warning(pop) // Restore warning state
