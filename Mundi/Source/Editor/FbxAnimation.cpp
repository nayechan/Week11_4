#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // Disable double to float conversion warning for FBX SDK
#include "FbxAnimation.h"
#include "FbxHelper.h"
#include "FbxConvert.h"
#include "FbxDataConverter.h"  // GetJointPostConversionMatrix용 (legacy)
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"
#include <algorithm>

// ========================================
// FBX 애니메이션 추출
// ========================================

/**
 * ExtractAnimation
 *
 * FBX AnimStack에서 애니메이션 데이터를 추출하여 UAnimSequence로 변환
 */
void FFbxAnimation::ExtractAnimation(
	FbxAnimStack* AnimStack,
	const FSkeleton* TargetSkeleton,
	UAnimSequence* OutAnim,
	bool bForceFrontXAxis)
{
	if (!AnimStack || !TargetSkeleton || !OutAnim)
	{
		UE_LOG("Error: Invalid parameters for ExtractAnimation");
		return;
	}

	// 1. AnimLayer 가져오기 (보통 첫 번째)
	int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
	if (LayerCount == 0)
	{
		UE_LOG("Error: AnimStack has no layers");
		return;
	}

	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	UE_LOG("Using animation layer: %s", AnimLayer->GetName());

	// 2. 시간 범위 추출
	FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
	FbxTime StartTime = TimeSpan.GetStart();
	FbxTime EndTime = TimeSpan.GetStop();

	float Duration = static_cast<float>(EndTime.GetSecondDouble() - StartTime.GetSecondDouble());

	if (Duration <= 0.0f)
	{
		UE_LOG("Error: Invalid animation duration: %f", Duration);
		return;
	}

	UE_LOG("Animation duration: %f seconds", Duration);

	// 3. FrameRate 설정 (30fps 기본, FBX에서 추출 가능)
	FFrameRate FrameRate(30, 1);  // 기본값
	// TODO: FBX TimeMode에서 실제 프레임레이트 추출 가능

	int32 NumFrames = static_cast<int32>(Duration * FrameRate.AsDecimal()) + 1;

	OutAnim->FrameRate = FrameRate;
	OutAnim->NumberOfFrames = NumFrames;
	OutAnim->SequenceLength = Duration;

	UE_LOG("Frame rate: %f fps, Frames: %d", FrameRate.AsDecimal(), NumFrames);

	// 4. RootNode부터 본별 애니메이션 추출
	FbxNode* RootNode = AnimStack->GetScene()->GetRootNode();
	UE_LOG("");
	UE_LOG("--- [SKELETON INFO] Target skeleton has %d bones ---", TargetSkeleton->Bones.Num());
	for (const auto& Pair : TargetSkeleton->BoneNameToIndex)
	{
		UE_LOG("    Skeleton Bone[%d]: '%s'", Pair.second, Pair.first.c_str());
	}
	UE_LOG("");
	UE_LOG("--- [FBX EXTRACTION] Searching for bones in FBX file ---");

	// 디버그용 본 카운터 (첫 3개만 자세히 로깅)
	int DebugBoneCount = 0;
	ExtractBoneCurves(RootNode, AnimLayer, TargetSkeleton, OutAnim, bForceFrontXAxis, DebugBoneCount);
	UE_LOG("");

	// 5. NumberOfKeys 계산
	int32 TotalKeys = 0;
	for (const FBoneAnimationTrack& Track : OutAnim->GetBoneAnimationTracks())
	{
		TotalKeys += Track.InternalTrack.GetNumKeys();
	}
	OutAnim->NumberOfKeys = TotalKeys;

	UE_LOG("--- [RESULT] Extracted %d bone tracks, %d total keys ---",
		   OutAnim->GetBoneAnimationTracks().Num(), TotalKeys);
	if (OutAnim->GetBoneAnimationTracks().Num() == 0)
	{
		UE_LOG("!!! WARNING: NO BONE TRACKS EXTRACTED !!!");
		UE_LOG("!!! This means bone names in FBX don't match skeleton bone names !!!");
	}
	UE_LOG("");
}

/**
 * ExtractBoneCurves
 *
 * FBX 씬을 재귀적으로 순회하며 Skeleton 노드를 찾아 애니메이션 커브 추출
 */
void FFbxAnimation::ExtractBoneCurves(
	FbxNode* InNode,
	FbxAnimLayer* AnimLayer,
	const FSkeleton* TargetSkeleton,
	UAnimSequence* OutAnim,
	bool bForceFrontXAxis,
	int& DebugBoneCount)
{
	if (!InNode || !AnimLayer || !TargetSkeleton || !OutAnim)
	{
		return;
	}

	// 1. 현재 노드가 본(Skeleton)인지 확인
	for (int i = 0; i < InNode->GetNodeAttributeCount(); i++)
	{
		FbxNodeAttribute* Attr = InNode->GetNodeAttributeByIndex(i);
		if (!Attr)
			continue;

		if (Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			// 본 발견! 애니메이션 커브 추출
			FString BoneName = FFbxHelper::GetFbxObjectName(InNode, false);
			UE_LOG("    FBX Bone found: '%s'", BoneName.c_str());

			// 본 매칭
			const int32* BoneIndexPtr = TargetSkeleton->BoneNameToIndex.Find(BoneName);
			if (!BoneIndexPtr)
			{
				UE_LOG("    -> [SKIP] NOT FOUND in skeleton (name mismatch)", BoneName.c_str());
				break;  // 스켈레톤에 없는 본은 스킵
			}

			int32 BoneIndex = *BoneIndexPtr;
			UE_LOG("    -> Matched to Skeleton Bone[%d]", BoneIndex);

			// 본 애니메이션 트랙 생성
			FBoneAnimationTrack Track;
			Track.Name = FName(BoneName);
			Track.BoneTreeIndex = BoneIndex;

			// 커브 추출
			ExtractCurveData(InNode, AnimLayer, TargetSkeleton, Track, bForceFrontXAxis, DebugBoneCount);

			// 키프레임이 있는 경우에만 추가
			if (!Track.InternalTrack.IsEmpty())
			{
				OutAnim->AddBoneTrack(Track);
				UE_LOG("    -> [SUCCESS] Extracted %d animation keys", Track.InternalTrack.GetNumKeys());
			}
			else
			{
				UE_LOG("    -> [SKIP] No animation keys found for this bone");
			}

			break;  // 노드당 1개의 Skeleton 속성만 있음
		}
	}

	// 2. Depth-first 재귀 (자식 노드 순회)
	for (int i = 0; i < InNode->GetChildCount(); i++)
	{
		ExtractBoneCurves(InNode->GetChild(i), AnimLayer, TargetSkeleton, OutAnim, bForceFrontXAxis, DebugBoneCount);
	}
}

/**
 * ExtractCurveData
 *
 * 특정 본의 애니메이션 커브에서 키프레임 데이터 추출
 */
void FFbxAnimation::ExtractCurveData(
	FbxNode* BoneNode,
	FbxAnimLayer* AnimLayer,
	const FSkeleton* TargetSkeleton,
	FBoneAnimationTrack& OutTrack,
	bool bForceFrontXAxis,
	int& DebugBoneCount)
{
	if (!BoneNode || !AnimLayer)
	{
		return;
	}

	bool bShowDebug = (DebugBoneCount < 4); // 처음 4개 본만 자세히 로그 (bip001 포함)
	DebugBoneCount++;

	FString BoneName = FFbxHelper::GetFbxObjectName(BoneNode, false);

	// 모든 애니메이션 커브에서 KeyTime 수집
	TArray<FbxTime> AllKeyTimes;
	TArray<FbxAnimCurve*> AllCurves = CollectUniqueKeyTimes(BoneNode, AnimLayer, AllKeyTimes);

	// 애니메이션 커브가 하나도 없으면 종료
	if (AllCurves.IsEmpty())
	{
		if (bShowDebug)
		{
			UE_LOG("       [CURVE DEBUG] Bone: '%s' - No animation curves", BoneName.c_str());
		}
		return;
	}

	if (bShowDebug)
	{
		UE_LOG("       [UE5 METHOD] Bone: '%s' - Using EvaluateGlobalTransform()", BoneName.c_str());
		UE_LOG("         Total unique keyframes: %d", AllKeyTimes.Num());
	}

	// Get FbxScene and Parent Node
	FbxScene* Scene = BoneNode->GetScene();
	FbxNode* ParentNode = BoneNode->GetParent();

	// Reserve space for animation tracks
	OutTrack.InternalTrack.PosKeys.Reserve(AllKeyTimes.Num());
	OutTrack.InternalTrack.RotKeys.Reserve(AllKeyTimes.Num());
	OutTrack.InternalTrack.ScaleKeys.Reserve(AllKeyTimes.Num());

	// Evaluate transforms at each KeyTime using Scene Graph (respects ConvertScene!)
	for (int32 KeyIndex = 0; KeyIndex < AllKeyTimes.Num(); ++KeyIndex)
	{
		const FbxTime& KeyTime = AllKeyTimes[KeyIndex];

		// 로컬 변환 평가
		FVector Position, Scale;
		FQuat Rotation;
		EvaluateLocalTransformAtTime(BoneNode, ParentNode, Scene, KeyTime, bForceFrontXAxis,
									  Position, Rotation, Scale);

		// NaN/Inf 검증
		ValidateTransform(Position, Rotation, Scale, KeyTime);

		// Debug logging for first keyframe of first 4 bones
		if (bShowDebug && KeyIndex == 0)
		{
			UE_LOG("         Key[0] Position: (%.3f, %.3f, %.3f)", Position.X, Position.Y, Position.Z);
			UE_LOG("         Key[0] Rotation Quat: (%.3f, %.3f, %.3f, %.3f)", Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
			UE_LOG("         Key[0] Scale: (%.3f, %.3f, %.3f)", Scale.X, Scale.Y, Scale.Z);
		}

		// Store keyframes
		OutTrack.InternalTrack.PosKeys.Add(Position);
		OutTrack.InternalTrack.RotKeys.Add(Rotation);
		OutTrack.InternalTrack.ScaleKeys.Add(Scale);
	}

	// Verify all tracks have same keyframe count
	int32 PosCount = OutTrack.InternalTrack.PosKeys.Num();
	int32 RotCount = OutTrack.InternalTrack.RotKeys.Num();
	int32 ScaleCount = OutTrack.InternalTrack.ScaleKeys.Num();

	if (bShowDebug)
	{
		UE_LOG("         Final keyframe counts - Pos: %d, Rot: %d, Scale: %d", PosCount, RotCount, ScaleCount);
	}

	if (PosCount != RotCount || PosCount != ScaleCount)
	{
		UE_LOG("Warning: Keyframe count mismatch for bone '%s': Pos=%d, Rot=%d, Scale=%d",
			   OutTrack.Name.ToString().c_str(), PosCount, RotCount, ScaleCount);
	}
}

/**
 * CollectUniqueKeyTimes
 *
 * 모든 애니메이션 커브에서 unique한 KeyTime 수집
 */
TArray<FbxAnimCurve*> FFbxAnimation::CollectUniqueKeyTimes(
	FbxNode* BoneNode,
	FbxAnimLayer* AnimLayer,
	TArray<FbxTime>& OutKeyTimes)
{
	TArray<FbxAnimCurve*> AllCurves;

	// Translation curves
	if (FbxAnimCurve* Curve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z))
		AllCurves.Add(Curve);

	// Rotation curves
	if (FbxAnimCurve* Curve = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z))
		AllCurves.Add(Curve);

	// Scale curves
	if (FbxAnimCurve* Curve = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y))
		AllCurves.Add(Curve);
	if (FbxAnimCurve* Curve = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z))
		AllCurves.Add(Curve);

	// 모든 커브에서 unique한 KeyTime 수집
	for (FbxAnimCurve* Curve : AllCurves)
	{
		int32 KeyCount = Curve->KeyGetCount();
		for (int32 i = 0; i < KeyCount; ++i)
		{
			FbxTime KeyTime = Curve->KeyGetTime(i);

			// 중복 체크
			bool bExists = false;
			for (const FbxTime& ExistingTime : OutKeyTimes)
			{
				if (KeyTime == ExistingTime)
				{
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				OutKeyTimes.Add(KeyTime);
			}
		}
	}

	// 시간순 정렬
	std::sort(OutKeyTimes.begin(), OutKeyTimes.end(), [](const FbxTime& a, const FbxTime& b) {
		return a < b;
	});

	return AllCurves;
}

/**
 * EvaluateLocalTransformAtTime
 *
 * 특정 시간에서 본의 로컬 변환 계산
 * UE5 패턴: EvaluateGlobalTransform + 부모 역변환 + JointPostConversionMatrix 적용
 */
void FFbxAnimation::EvaluateLocalTransformAtTime(
	FbxNode* BoneNode,
	FbxNode* ParentNode,
	FbxScene* Scene,
	FbxTime KeyTime,
	bool bForceFrontXAxis,
	FVector& OutPosition,
	FQuat& OutRotation,
	FVector& OutScale)
{
	// ========================================
	// UE5 Pattern: Conditional JointPostConversionMatrix for Animation
	// ========================================
	// Reference: UE5 FbxMainImport.cpp Line 1559-1562
	//
	// bForceFrontXAxis = true  → Apply JointPost (-90°, -90°, 0°) for +X Forward conversion
	// bForceFrontXAxis = false → Identity (using -Y Forward, no additional rotation needed)
	//
	// UE5 applies JointOrientationMatrix ONLY when bForceFrontXAxis is enabled
	FbxAMatrix JointPostMatrix = FFbxDataConverter::GetJointPostConversionMatrix(bForceFrontXAxis);

	// Get global transform at this time
	FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, KeyTime);
	GlobalTransform = GlobalTransform * JointPostMatrix;

	// Compute local transform relative to parent
	FbxAMatrix LocalTransform;
	if (ParentNode)
	{
		FbxAMatrix ParentGlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(ParentNode, KeyTime);
		// Apply JointPostConversionMatrix to parent as well (same flag)
		ParentGlobalTransform = ParentGlobalTransform * JointPostMatrix;
		LocalTransform = ParentGlobalTransform.Inverse() * GlobalTransform;
	}
	else
	{
		// Root bone: use global transform directly
		LocalTransform = GlobalTransform;
	}

	// Extract Position with Y-Flip (Right-Handed → Left-Handed)
	FbxVector4 Translation = LocalTransform.GetT();
	OutPosition = FFbxConvert::ConvertPos(Translation);

	// Extract Rotation with Y-Flip (Right-Handed → Left-Handed)
	FbxQuaternion FbxQuat = LocalTransform.GetQ();
	OutRotation = FFbxConvert::ConvertRotation(FbxQuat);
	OutRotation.Normalize();

	// Extract Scale (NO Y-Flip needed for scale)
	FbxVector4 Scaling = LocalTransform.GetS();
	OutScale = FFbxConvert::ConvertScale(Scaling);
}

/**
 * ValidateTransform
 *
 * 변환 값의 NaN/Inf 검증 및 수정
 */
bool FFbxAnimation::ValidateTransform(
	FVector& Position,
	FQuat& Rotation,
	FVector& Scale,
	FbxTime KeyTime)
{
	bool bHasInvalidValues = false;

	// Position 검증
	if (std::isnan(Position.X) || std::isnan(Position.Y) || std::isnan(Position.Z) ||
		std::isinf(Position.X) || std::isinf(Position.Y) || std::isinf(Position.Z))
	{
		float TimeSeconds = static_cast<float>(KeyTime.GetSecondDouble());
		UE_LOG("Warning: Invalid position value at time %f, using zero", TimeSeconds);
		Position = FVector(0, 0, 0);
		bHasInvalidValues = true;
	}

	// Rotation 검증
	if (std::isnan(Rotation.X) || std::isnan(Rotation.Y) || std::isnan(Rotation.Z) || std::isnan(Rotation.W) ||
		std::isinf(Rotation.X) || std::isinf(Rotation.Y) || std::isinf(Rotation.Z) || std::isinf(Rotation.W))
	{
		float TimeSeconds = static_cast<float>(KeyTime.GetSecondDouble());
		UE_LOG("Warning: Invalid rotation value at time %f, using identity", TimeSeconds);
		Rotation = FQuat::Identity();
		bHasInvalidValues = true;
	}

	// Scale 검증
	if (std::isnan(Scale.X) || std::isnan(Scale.Y) || std::isnan(Scale.Z) ||
		std::isinf(Scale.X) || std::isinf(Scale.Y) || std::isinf(Scale.Z))
	{
		float TimeSeconds = static_cast<float>(KeyTime.GetSecondDouble());
		UE_LOG("Warning: Invalid scale value at time %f, using (1,1,1)", TimeSeconds);
		Scale = FVector(1.0f, 1.0f, 1.0f);
		bHasInvalidValues = true;
	}

	return bHasInvalidValues;
}

#pragma warning(pop) // Restore warning state
