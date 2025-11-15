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
	FFbxScene::ExtractSkeleton(RootNode, *MeshData, BoneToIndex);

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
			CachedScene.bForceFrontXAxis
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

	// 3. UAnimSequence 생성
	UAnimSequence* AnimSeq = NewObject<UAnimSequence>();
	AnimSeq->SetFilePath(FilePath);
	AnimSeq->Skeleton = const_cast<FSkeleton*>(TargetSkeleton);

	// 4. 첫 번째 AnimStack 로드
	FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
	UE_LOG("========================================");
	UE_LOG("[ANIMATION] FFbxParser: Loading from FBX: %s", FilePath.c_str());
	UE_LOG("[ANIMATION] Animation stack name: %s", AnimStack->GetName());
	UE_LOG("========================================");

	// 5. 애니메이션 추출 (Phase 4: FFbxAnimation)
	FFbxAnimation::ExtractAnimation(AnimStack, TargetSkeleton, AnimSeq, CachedScene.bForceFrontXAxis);

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

	// 4. FbxScene 생성 및 Import
	FbxScene* Scene = FbxScene::Create(SDKManager, "Shared Scene");
	SDKImporter->Import(Scene);
	SDKImporter->Destroy();
	SDKImporter = nullptr;

	// 5. 좌표계 변환
	bool bForceFrontXAxis = true;  // UE5 default: false, Mundi: true
	ConvertScene(Scene, bForceFrontXAxis);

	// 6. 단위 변환 (meter로 변환)
	FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
	double ScaleFactor = SceneSystemUnit.GetScaleFactor();

	if (ScaleFactor != 100.0)
	{
		FbxSystemUnit::m.ConvertScene(Scene);
		Scene->GetAnimationEvaluator()->Reset();  // Reset evaluator after unit conversion
	}

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
	// UE5 Pattern: Coordinate System Conversion
	// ========================================
	// Unreal Engine uses -Y as forward axis by default to mimic Maya/Max behavior
	// This way, models facing +X in DCC tools will face +X in engine
	//
	// Reference: UE5 Engine\Source\Editor\UnrealEd\Private\Fbx\FbxMainImport.cpp Line 1528-1563
	//
	// Quote from UE5 source:
	// "we use -Y as forward axis here when we import. This is odd considering our
	//  forward axis is technically +X but this is to mimic Maya/Max behavior where
	//  if you make a model facing +X facing, when you import that mesh, you want
	//  +X facing in engine."

	// Default: -Y Forward (matches Maya/Max workflow)
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector) - FbxAxisSystem::eParityOdd;

	// bForceFrontXAxis option: use +X Forward instead (default: false in UE5)
	if (bForceFrontXAxis)
	{
		FrontVector = FbxAxisSystem::eParityEven;  // +X Forward
	}

	// Target coordinate system: Z-Up, Right-Handed, with dynamic Front vector
	FbxAxisSystem UnrealImportAxis(
		FbxAxisSystem::eZAxis,           // Up: Z
		FrontVector,                     // Front: -Y (default) or +X (if bForceFrontXAxis)
		FbxAxisSystem::eRightHanded      // Right-Handed intermediate stage
	);

	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	if (SourceSetup != UnrealImportAxis)
	{
		// UE5 Pattern: Remove all FBX roots before conversion
		FbxRootNodeUtility::RemoveAllFbxRoots(Scene);

		// CHANGED: DeepConvertScene → ConvertScene
		// ConvertScene only transforms Scene Graph, NOT vertex data
		// This prevents double-transformation bug (100x scale issue)
		UnrealImportAxis.ConvertScene(Scene);
		Scene->GetAnimationEvaluator()->Reset();  // Reset evaluator after conversion
	}

	// ========================================
	// JointOrientationMatrix 설정 (UE5 Pattern)
	// ========================================
	// IMPORTANT: JointOrientationMatrix는 함수가 아니라 public 멤버 변수!
	//
	// UE5 Reference: FbxAPI.cpp:480-500 (FFbxParser::JointPostConversionMatrix)
	//
	// 용도: BindPose 계산 시 사용
	// GlobalBindPoseMatrix = TransformLinkMatrix * JointOrientationMatrix

	if (bForceFrontXAxis)
	{
		// +X Forward: 90도 회전 행렬 적용
		// UE5 Pattern: FbxMathInterface::GetJointPostConversionMatrix(...)
		// 하지만 Mundi는 단순화하여 Identity 사용 (좌표계 변환은 FFbxConvert에서 처리)
		JointOrientationMatrix.SetIdentity();
	}
	else
	{
		// -Y Forward (기본값): Identity
		JointOrientationMatrix.SetIdentity();
	}
}

#pragma warning(pop) // Restore warning state
