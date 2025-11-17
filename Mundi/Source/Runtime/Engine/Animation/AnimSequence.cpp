#include "pch.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"
#include "Source/Runtime/Core/Misc/VertexData.h" 

void UAnimSequence::GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context)
{
	// 스켈레톤이 없으면 실패
	if (!Skeleton)
	{
		UE_LOG("UAnimSequence::GetAnimationPose - No skeleton assigned");
		return;
	}

	// 본 개수만큼 포즈 초기화
	const int32 NumBones = Skeleton->Bones.Num();
	OutPose.SetNumBones(NumBones);

	// 모든 본에 대해 애니메이션 트랙에서 현재 시간의 트랜스폼 추출
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		OutPose.BoneTransforms[BoneIndex] = GetBoneTransformAtTime(BoneIndex, Context.CurrentTime);
	}	
}

FTransform UAnimSequence::GetBoneTransformAtTime(int32 BoneIndex, float Time) const
{
	// 스켈레톤 범위 체크
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
	{
		return FTransform();
	}

	// BoneAnimationTracks 배열에서 BoneTreeIndex로 매칭되는 트랙 찾기
	const FBoneAnimationTrack* Track = nullptr;
	for (const FBoneAnimationTrack& T : BoneAnimationTracks)
	{
		if (T.BoneTreeIndex == BoneIndex)
		{
			Track = &T;
			break;
		}
	}

	// 애니메이션 트랙이 없으면 identity (T-Pose 유지)
	if (!Track || Track->InternalTrack.IsEmpty())
	{
		return FTransform();
	}

	const FRawAnimSequenceTrack& RawTrack = Track->InternalTrack;

	// Time은 StateMachine::Update()에서 이미 Loop 처리됨 (UE5 패턴)
	// 여기서는 받은 Time을 그대로 사용

	// 각 컴포넌트 보간
	FVector Position = InterpolatePosition(RawTrack.PosKeys, Time);
	FQuat Rotation = InterpolateRotation(RawTrack.RotKeys, Time);
	FVector Scale = InterpolateScale(RawTrack.ScaleKeys, Time);

	return FTransform(Position, Rotation, Scale);
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FVector(0, 0, 0);

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// 선형 보간
	return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FQuat();

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// Spherical Linear Interpolation (Slerp)
	return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FVector(1, 1, 1);

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// 선형 보간
	return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

void UAnimSequence::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// FrameRate 로드
		FJsonSerializer::ReadInt32(InOutHandle, "FrameRate_Numerator", FrameRate.Numerator, 30, false);
		FJsonSerializer::ReadInt32(InOutHandle, "FrameRate_Denominator", FrameRate.Denominator, 1, false);

		// NumberOfFrames, NumberOfKeys 로드
		FJsonSerializer::ReadInt32(InOutHandle, "NumberOfFrames", NumberOfFrames, 0, false);
		FJsonSerializer::ReadInt32(InOutHandle, "NumberOfKeys", NumberOfKeys, 0, false);

		// BoneAnimationTracks 로드
		JSON TracksJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "BoneAnimationTracks", TracksJson, nullptr, false))
		{
			BoneAnimationTracks.clear();
			for (uint32 i = 0; i < static_cast<uint32>(TracksJson.size()); ++i)
			{
				JSON TrackJson = TracksJson.at(i);
				FBoneAnimationTrack Track;

				// Track 기본 정보
				FString BoneName;
				FJsonSerializer::ReadString(TrackJson, "BoneName", BoneName, "", false);
				Track.Name = FName(BoneName);
				FJsonSerializer::ReadInt32(TrackJson, "BoneTreeIndex", Track.BoneTreeIndex, -1, false);

				// PosKeys 로드
				JSON PosKeysJson;
				if (FJsonSerializer::ReadArray(TrackJson, "PosKeys", PosKeysJson, nullptr, false))
				{
					Track.InternalTrack.PosKeys.clear();
					for (uint32 j = 0; j < static_cast<uint32>(PosKeysJson.size()); ++j)
					{
						JSON KeyJson = PosKeysJson.at(j);
						FVector Key;
						FJsonSerializer::ReadFloat(KeyJson, "X", Key.X, 0.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Y", Key.Y, 0.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Z", Key.Z, 0.0f, false);
						Track.InternalTrack.PosKeys.Add(Key);
					}
				}

				// RotKeys 로드
				JSON RotKeysJson;
				if (FJsonSerializer::ReadArray(TrackJson, "RotKeys", RotKeysJson, nullptr, false))
				{
					Track.InternalTrack.RotKeys.clear();
					for (uint32 j = 0; j < static_cast<uint32>(RotKeysJson.size()); ++j)
					{
						JSON KeyJson = RotKeysJson.at(j);
						FQuat Key;
						FJsonSerializer::ReadFloat(KeyJson, "X", Key.X, 0.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Y", Key.Y, 0.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Z", Key.Z, 0.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "W", Key.W, 1.0f, false);
						Track.InternalTrack.RotKeys.Add(Key);
					}
				}

				// ScaleKeys 로드
				JSON ScaleKeysJson;
				if (FJsonSerializer::ReadArray(TrackJson, "ScaleKeys", ScaleKeysJson, nullptr, false))
				{
					Track.InternalTrack.ScaleKeys.clear();
					for (uint32 j = 0; j < static_cast<uint32>(ScaleKeysJson.size()); ++j)
					{
						JSON KeyJson = ScaleKeysJson.at(j);
						FVector Key;
						FJsonSerializer::ReadFloat(KeyJson, "X", Key.X, 1.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Y", Key.Y, 1.0f, false);
						FJsonSerializer::ReadFloat(KeyJson, "Z", Key.Z, 1.0f, false);
						Track.InternalTrack.ScaleKeys.Add(Key);
					}
				}

				BoneAnimationTracks.Add(Track);
			}
		}
	}
	else
	{
		// FrameRate 저장
		InOutHandle["FrameRate_Numerator"] = FrameRate.Numerator;
		InOutHandle["FrameRate_Denominator"] = FrameRate.Denominator;

		// NumberOfFrames, NumberOfKeys 저장
		InOutHandle["NumberOfFrames"] = NumberOfFrames;
		InOutHandle["NumberOfKeys"] = NumberOfKeys;

		// BoneAnimationTracks 저장
		JSON TracksArray = JSON::Make(JSON::Class::Array);
		for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
		{
			JSON TrackJson;
			TrackJson["BoneName"] = Track.Name.ToString();
			TrackJson["BoneTreeIndex"] = Track.BoneTreeIndex;

			// PosKeys 저장
			JSON PosKeysArray = JSON::Make(JSON::Class::Array);
			for (const FVector& Key : Track.InternalTrack.PosKeys)
			{
				JSON KeyJson;
				KeyJson["X"] = Key.X;
				KeyJson["Y"] = Key.Y;
				KeyJson["Z"] = Key.Z;
				PosKeysArray.append(KeyJson);
			}
			TrackJson["PosKeys"] = PosKeysArray;

			// RotKeys 저장
			JSON RotKeysArray = JSON::Make(JSON::Class::Array);
			for (const FQuat& Key : Track.InternalTrack.RotKeys)
			{
				JSON KeyJson;
				KeyJson["X"] = Key.X;
				KeyJson["Y"] = Key.Y;
				KeyJson["Z"] = Key.Z;
				KeyJson["W"] = Key.W;
				RotKeysArray.append(KeyJson);
			}
			TrackJson["RotKeys"] = RotKeysArray;

			// ScaleKeys 저장
			JSON ScaleKeysArray = JSON::Make(JSON::Class::Array);
			for (const FVector& Key : Track.InternalTrack.ScaleKeys)
			{
				JSON KeyJson;
				KeyJson["X"] = Key.X;
				KeyJson["Y"] = Key.Y;
				KeyJson["Z"] = Key.Z;
				ScaleKeysArray.append(KeyJson);
			}
			TrackJson["ScaleKeys"] = ScaleKeysArray;

			TracksArray.append(TrackJson);
		}
		InOutHandle["BoneAnimationTracks"] = TracksArray;
	}
}

bool UAnimSequence::SaveToFile(const FString& FilePath)
{
	try
	{
		JSON RootJson;

		// Serialize all data to JSON
		Serialize(false, RootJson);

		// Write to file
		std::ofstream OutFile(FilePath);
		if (!OutFile.is_open())
		{
			UE_LOG("Failed to open file for writing: %s", FilePath.c_str());
			return false;
		}

		OutFile << RootJson.dump(4);  // Pretty print with 4 spaces
		OutFile.close();

		UE_LOG("Successfully saved AnimSequence to: %s", FilePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG("Exception saving AnimSequence: %s", e.what());
		return false;
	}
}

bool UAnimSequence::LoadFromFile(const FString& FilePath)
{
	try
	{
		std::ifstream InFile(FilePath);
		if (!InFile.is_open())
		{
			UE_LOG("Failed to open file for reading: %s", FilePath.c_str());
			return false;
		}

		FString JsonString((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
		InFile.close();

		JSON RootJson = JSON::Load(JsonString);

		// Deserialize from JSON
		Serialize(true, RootJson);

		UE_LOG("Successfully loaded AnimSequence from: %s", FilePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG("Exception loading AnimSequence: %s", e.what());
		return false;
	}
}

bool UAnimSequence::SaveNotifyData(const FString& FilePath)
{
	try
	{
		JSON RootJson;

		// 원본 파일 경로를 상대경로로 저장 (복원용)
		FString SourcePath = GetFilePath();
		if (!SourcePath.empty())
		{
			// 절대경로를 상대경로로 변환
			std::filesystem::path AbsPath(SourcePath);
			std::filesystem::path CurrentPath = std::filesystem::current_path();

			try
			{
				std::filesystem::path RelPath = std::filesystem::relative(AbsPath, CurrentPath);
				RootJson["SourceFilePath"] = RelPath.string();
			}
			catch (...)
			{
				// relative() 실패 시 원본 경로 사용
				RootJson["SourceFilePath"] = SourcePath;
			}
		}
		else
		{
			RootJson["SourceFilePath"] = "";
		}

		// 애니메이션 기본 정보
		RootJson["SequenceLength"] = SequenceLength;
		RootJson["NumberOfFrames"] = NumberOfFrames;

		// Notifies 저장
		JSON NotifiesArray = JSON::Make(JSON::Class::Array);
		for (const FAnimNotifyEvent& Notify : Notifies)
		{
			JSON NotifyJson;
			NotifyJson["TriggerTime"] = Notify.TriggerTime;
			NotifyJson["Duration"] = Notify.Duration;
			NotifyJson["NotifyName"] = Notify.NotifyName.ToString();
			NotifyJson["NotifyData"] = Notify.NotifyData;
			NotifyJson["TrackIndex"] = Notify.TrackIndex;
			NotifiesArray.append(NotifyJson);
		}
		RootJson["Notifies"] = NotifiesArray;

		// NotifyTracks 저장
		JSON TracksArray = JSON::Make(JSON::Class::Array);
		for (const UAnimSequenceBase::FNotifyTrack& Track : NotifyTracks)
		{
			JSON TrackJson;
			TrackJson["ID"] = Track.ID;
			TrackJson["Name"] = Track.Name;
			TracksArray.append(TrackJson);
		}
		RootJson["NotifyTracks"] = TracksArray;
		RootJson["NextTrackID"] = NextTrackID;

		// 파일에 저장
		std::ofstream OutFile(FilePath);
		if (!OutFile.is_open())
		{
			UE_LOG("Failed to open file for writing: %s", FilePath.c_str());
			return false;
		}

		OutFile << RootJson.dump(4);
		OutFile.close();

		UE_LOG("Successfully saved Notify data to: %s", FilePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG("Exception saving Notify data: %s", e.what());
		return false;
	}
}

bool UAnimSequence::LoadNotifyData(const FString& FilePath)
{
	try
	{
		std::ifstream InFile(FilePath);
		if (!InFile.is_open())
		{
			UE_LOG("Failed to open file for reading: %s", FilePath.c_str());
			return false;
		}

		FString JsonString((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
		InFile.close();

		JSON RootJson = JSON::Load(JsonString);

		// Notifies 로드
		Notifies.clear();
		JSON NotifiesJson;
		if (FJsonSerializer::ReadArray(RootJson, "Notifies", NotifiesJson, nullptr, false))
		{
			for (uint32 i = 0; i < static_cast<uint32>(NotifiesJson.size()); ++i)
			{
				JSON NotifyJson = NotifiesJson.at(i);

				FAnimNotifyEvent Notify;
				FJsonSerializer::ReadFloat(NotifyJson, "TriggerTime", Notify.TriggerTime, 0.0f, false);
				FJsonSerializer::ReadFloat(NotifyJson, "Duration", Notify.Duration, 0.0f, false);

				FString NotifyNameStr;
				FJsonSerializer::ReadString(NotifyJson, "NotifyName", NotifyNameStr, "", false);
				Notify.NotifyName = FName(NotifyNameStr.c_str());

				FJsonSerializer::ReadString(NotifyJson, "NotifyData", Notify.NotifyData, "", false);
				FJsonSerializer::ReadInt32(NotifyJson, "TrackIndex", Notify.TrackIndex, 0, false);

				Notifies.Add(Notify);
			}
		}

		// NotifyTracks 로드
		NotifyTracks.clear();
		JSON TracksJson;
		if (FJsonSerializer::ReadArray(RootJson, "NotifyTracks", TracksJson, nullptr, false))
		{
			for (uint32 i = 0; i < static_cast<uint32>(TracksJson.size()); ++i)
			{
				JSON TrackJson = TracksJson.at(i);
				UAnimSequenceBase::FNotifyTrack Track;
				FJsonSerializer::ReadInt32(TrackJson, "ID", Track.ID, 0, false);
				FJsonSerializer::ReadString(TrackJson, "Name", Track.Name, "", false);
				NotifyTracks.Add(Track);
			}
		}
		FJsonSerializer::ReadInt32(RootJson, "NextTrackID", NextTrackID, 1, false);

		UE_LOG("Successfully loaded Notify data from: %s", FilePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG("Exception loading Notify data: %s", e.what());
		return false;
	}
}
