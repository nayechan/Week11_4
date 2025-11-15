#pragma once
#include "AnimSequenceBase.h"
#include "UAnimSequence.generated.h"

UCLASS(DisplayName="애니메이션 시퀀스", Description="키프레임 애니메이션 데이터")
class UAnimSequence : public UAnimSequenceBase
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimSequence() = default;
	virtual ~UAnimSequence() = default;

	// 프레임 레이트
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="프레임 레이트")
	FFrameRate FrameRate;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="총 프레임 수")
	int32 NumberOfFrames = 0;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="총 키 개수")
	int32 NumberOfKeys = 0;

	// 포즈 추출 구현
	virtual void GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context) override;

	// 특정 시간의 본 트랜스폼 가져오기 (보간)
	FTransform GetBoneTransformAtTime(int32 BoneIndex, float Time) const;

	// 본 애니메이션 트랙 접근자
	const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const { return BoneAnimationTracks; }

	// 본 트랙 추가 (FBX Loader가 사용)
	void AddBoneTrack(const FBoneAnimationTrack& Track) { BoneAnimationTracks.Add(Track); }
	void SetBoneTracks(const TArray<FBoneAnimationTrack>& Tracks) { BoneAnimationTracks = Tracks; }

	// 직렬화 (JSON)
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 바이너리 직렬화 (캐싱용)
	friend FArchive& operator<<(FArchive& Ar, UAnimSequence& Anim)
	{
		if (Ar.IsSaving())
		{
			// 1. FrameRate 저장
			Ar << Anim.FrameRate;

			// 2. 프레임 정보 저장
			Ar << Anim.NumberOfFrames;
			Ar << Anim.NumberOfKeys;

			// 3. SequenceLength 저장 (부모 클래스 프로퍼티)
			Ar << Anim.SequenceLength;

			// 4. BoneAnimationTracks 저장
			uint32 TrackCount = static_cast<uint32>(Anim.BoneAnimationTracks.Num());
			Ar << TrackCount;
			for (auto& Track : Anim.BoneAnimationTracks)
			{
				Ar << Track;
			}
		}
		else if (Ar.IsLoading())
		{
			// 1. FrameRate 로드
			Ar << Anim.FrameRate;

			// 2. 프레임 정보 로드
			Ar << Anim.NumberOfFrames;
			Ar << Anim.NumberOfKeys;

			// 3. SequenceLength 로드
			Ar << Anim.SequenceLength;

			// 4. BoneAnimationTracks 로드
			uint32 TrackCount;
			Ar << TrackCount;

			// Sanity check
			if (TrackCount > Serialization::MAX_REASONABLE_ARRAY_SIZE)
			{
				throw std::runtime_error("Cache corrupt: Animation track count is unreasonable.");
			}

			Anim.BoneAnimationTracks.SetNum(TrackCount);
			for (uint32 i = 0; i < TrackCount; ++i)
			{
				Ar << Anim.BoneAnimationTracks[i];
			}
		}
		return Ar;
	}

private:
	// 본별 애니메이션 트랙
	TArray<FBoneAnimationTrack> BoneAnimationTracks;

	// FBX Loader가 데이터를 채울 수 있도록
	friend class UFbxLoader;

	// 보간 헬퍼 함수
	FVector InterpolatePosition(const TArray<FVector>& Keys, float Time) const;
	FQuat InterpolateRotation(const TArray<FQuat>& Keys, float Time) const;
	FVector InterpolateScale(const TArray<FVector>& Keys, float Time) const;
};
