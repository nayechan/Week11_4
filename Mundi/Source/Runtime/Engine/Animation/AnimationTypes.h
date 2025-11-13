#pragma once
#include "Vector.h"
#include "UEContainer.h"

// 프레임 레이트 구조체
struct FFrameRate
{
	int32 Numerator = 30;
	int32 Denominator = 1;

	float AsDecimal() const
	{
		return static_cast<float>(Numerator) / static_cast<float>(Denominator);
	}

	// 시간 → 프레임 변환
	int32 AsFrameNumber(float TimeInSeconds) const
	{
		return static_cast<int32>(TimeInSeconds * AsDecimal());
	}

	// 프레임 → 시간 변환
	float AsSeconds(int32 FrameNumber) const
	{
		return static_cast<float>(FrameNumber) / AsDecimal();
	}
};

// Raw 애니메이션 키프레임 (발제 문서 기준)
struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;      // 위치 키프레임
	TArray<FQuat> RotKeys;        // 회전 키프레임 (Quaternion)
	TArray<FVector> ScaleKeys;    // 스케일 키프레임

	// 비어있는지 확인
	bool IsEmpty() const
	{
		return PosKeys.IsEmpty() && RotKeys.IsEmpty() && ScaleKeys.IsEmpty();
	}

	// 키 개수 (가장 많은 키를 가진 트랙 기준)
	int32 GetNumKeys() const
	{
		int32 MaxKeys = 0;
		if (!PosKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, PosKeys.Num());
		if (!RotKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, RotKeys.Num());
		if (!ScaleKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, ScaleKeys.Num());
		return MaxKeys;
	}
};

// 본별 애니메이션 트랙 (발제 문서 기준)
struct FBoneAnimationTrack
{
	FName Name;                           // Bone 이름
	int32 BoneTreeIndex = -1;             // 스켈레톤 본 인덱스
	FRawAnimSequenceTrack InternalTrack;  // 실제 애니메이션 데이터

	FBoneAnimationTrack() = default;
	FBoneAnimationTrack(const FName& InName, int32 InBoneIndex)
		: Name(InName), BoneTreeIndex(InBoneIndex) {}
};

// AnimNotify 이벤트 (발제 문서 요구사항)
struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;     // 트리거 시간 (초)
	float Duration = 0.0f;         // 지속 시간 (0 = 순간 이벤트)
	FName NotifyName;              // Notify 이름 (예: "Footstep", "Shoot")
	FString NotifyData;            // 추가 데이터 (JSON 등)

	FAnimNotifyEvent() = default;
	FAnimNotifyEvent(float InTime, const FName& InName)
		: TriggerTime(InTime), NotifyName(InName) {}
};

// 애니메이션 추출 컨텍스트 (팀원2가 사용)
struct FAnimExtractContext
{
	float CurrentTime = 0.0f;          // 현재 시간 (초)
	bool bExtractRootMotion = false;   // 루트 모션 추출 여부
	bool bLooping = false;              // 루핑 여부

	FAnimExtractContext() = default;
	FAnimExtractContext(float InTime, bool InLooping)
		: CurrentTime(InTime), bLooping(InLooping) {}
};

// 포즈 데이터 컨테이너 (팀원2가 블렌딩에 사용)
struct FPoseContext
{
	TArray<FTransform> BoneTransforms;  // 모든 본의 로컬 트랜스폼

	FPoseContext() = default;

	void SetNumBones(int32 NumBones)
	{
		BoneTransforms.SetNum(NumBones);
		// Identity transform으로 초기화
		for (int32 i = 0; i < NumBones; ++i)
		{
			BoneTransforms[i] = FTransform();
		}
	}

	int32 GetNumBones() const { return BoneTransforms.Num(); }
};

// 애니메이션 모드 열거형
enum class EAnimationMode : uint8
{
	AnimationSingleNode,   // 단일 애니메이션 재생
	AnimationBlueprint,    // 애니메이션 블루프린트 (미래 확장)
};
