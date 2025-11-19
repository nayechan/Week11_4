#pragma once
#include "AnimSequenceBase.h"
#include "UAnimSequence.generated.h"

UCLASS(DisplayName = "애니메이션 시퀀스", Description = "키프레임 애니메이션 데이터")
class UAnimSequence : public UAnimSequenceBase
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimSequence() = default;
	virtual ~UAnimSequence() override = default;

	// 프레임 레이트
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="프레임 레이트")
	FFrameRate FrameRate;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="총 프레임 수")
	int32 NumberOfFrames = 0;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="총 키 개수")
	int32 NumberOfKeys = 0;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="역재생 여부")
	bool bReversePlay = false;

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

	// 파일 저장/로드 (전체 데이터 - BoneAnimationTracks 포함, 큰 파일)
	bool SaveToFile(const FString& FilePath);
	bool LoadFromFile(const FString& FilePath);

	// Notify 데이터만 저장/로드 (가벼운 파일, 추천)
	bool SaveNotifyData(const FString& FilePath);
	bool LoadNotifyData(const FString& FilePath);

	// 바이너리 직렬화 (캐싱용)
	friend FArchive& operator<<(FArchive& Ar, UAnimSequence& Anim)
	{
		if (Ar.IsSaving())
		{
			// 0. 원본 FBX 경로 저장 (Skeleton 복원용)
			FString SourceFBXPath = Anim.GetFilePath();
			Serialization::WriteString(Ar, SourceFBXPath);

			// 1. FrameRate 저장
			Ar << Anim.FrameRate;

			// 2. 프레임 정보 저장
			Ar << Anim.NumberOfFrames;
			Ar << Anim.NumberOfKeys;

			// 3. SequenceLength, RateScale 저장 (부모 클래스 프로퍼티)
			Ar << Anim.SequenceLength;
			Ar << Anim.RateScale;

			// 3-1. bReversePlay 저장
			Ar << Anim.bReversePlay;

			// 4. BoneAnimationTracks 저장
			uint32 TrackCount = static_cast<uint32>(Anim.BoneAnimationTracks.Num());
			Ar << TrackCount;
			for (auto& Track : Anim.BoneAnimationTracks)
			{
				Ar << Track;
			}

			// 5. Notify 정보 저장 (부모 클래스의 Notifies)
			uint32 NotifyCount = static_cast<uint32>(Anim.Notifies.Num());
			Ar << NotifyCount;
			for (auto& Notify : Anim.Notifies)
			{
				Ar << Notify;
			}

			// 6. NotifyTracks 저장 (부모 클래스)
			uint32 NotifyTrackCount = static_cast<uint32>(Anim.NotifyTracks.Num());
			Ar << NotifyTrackCount;
			for (auto& Track : Anim.NotifyTracks)
			{
				Ar << Track;
			}

			// 7. NextTrackID 저장 (부모 클래스)
			Ar << Anim.NextTrackID;

			// 8. CurveData 저장 (부모 클래스)
			Ar << Anim.CurveData;
		}
		else if (Ar.IsLoading())
		{
			// 0. 원본 FBX 경로 로드 (Skeleton 복원용)
			FString SourceFBXPath;
			Serialization::ReadString(Ar, SourceFBXPath);
			Anim.SetFilePath(SourceFBXPath);

			// 1. FrameRate 로드
			Ar << Anim.FrameRate;

			// 2. 프레임 정보 로드
			Ar << Anim.NumberOfFrames;
			Ar << Anim.NumberOfKeys;

			// 3. SequenceLength, RateScale 로드
			Ar << Anim.SequenceLength;
			Ar << Anim.RateScale;

			// 3-1. bReversePlay 로드
			Ar << Anim.bReversePlay;

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

			// 5. Notify 정보 로드
			uint32 NotifyCount;
			Ar << NotifyCount;

			// Sanity check
			if (NotifyCount > Serialization::MAX_REASONABLE_ARRAY_SIZE)
			{
				throw std::runtime_error("Cache corrupt: Notify count is unreasonable.");
			}

			Anim.Notifies.SetNum(NotifyCount);
			for (uint32 i = 0; i < NotifyCount; ++i)
			{
				Ar << Anim.Notifies[i];
			}

			// 6. NotifyTracks 로드
			uint32 NotifyTrackCount;
			Ar << NotifyTrackCount;

			// Sanity check
			if (NotifyTrackCount > Serialization::MAX_REASONABLE_ARRAY_SIZE)
			{
				throw std::runtime_error("Cache corrupt: NotifyTrack count is unreasonable.");
			}

			Anim.NotifyTracks.SetNum(NotifyTrackCount);
			for (uint32 i = 0; i < NotifyTrackCount; ++i)
			{
				Ar << Anim.NotifyTracks[i];
			}

			// 7. NextTrackID 로드
			Ar << Anim.NextTrackID;

			// 8. CurveData 로드 (부모 클래스)
			Ar << Anim.CurveData;
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
