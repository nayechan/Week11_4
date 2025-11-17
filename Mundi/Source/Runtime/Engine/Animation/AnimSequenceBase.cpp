#include "pch.h"
#include "AnimSequenceBase.h"
#include "GlobalConsole.h"

void UAnimSequenceBase::GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies) const
{
	OutNotifies.Empty();

	// Looping 애니메이션에서 시간이 wrap-around되는 경우 처리
	// 예: 프레임 N (0.95초) → 프레임 N+1 (0.02초)
	//     범위 [0.95, 0.02]는 0.95~1.0 + 0.0~0.02를 의미함
	bool bWrappedAround = EndTime < StartTime;

	for (const FAnimNotifyEvent& Notify : Notifies)
	{
		float TriggerTime = Notify.TriggerTime;

		if (bWrappedAround)
		{
			// Wrap-around: StartTime 초과 또는 EndTime 이하
			// 0.95 초과 (0.96~1.0) 또는 0.02 이하 (0.0~0.02)
			if (TriggerTime > StartTime || TriggerTime <= EndTime)
			{
				OutNotifies.Add(Notify);
			}
		}
		else
		{
			// 정상 범위: StartTime 초과 AND EndTime 이하
			// StartTime은 이전 프레임에서 이미 처리되었으므로 > 사용 (중복 방지)
			if (TriggerTime > StartTime && TriggerTime <= EndTime)
			{
				OutNotifies.Add(Notify);
			}
		}
	}
}

void UAnimSequenceBase::AddNotify(float TriggerTime, const FName& NotifyName, const FString& NotifyData)
{
	FAnimNotifyEvent NewNotify;
	NewNotify.TriggerTime = TriggerTime;
	NewNotify.NotifyName = NotifyName;
	NewNotify.NotifyData = NotifyData;
	NewNotify.Duration = 0.0f;

	Notifies.Add(NewNotify);

	UE_LOG("AnimSequenceBase: Added Notify '%s' at %.2fs", NotifyName.ToString().c_str(), TriggerTime);
}

void UAnimSequenceBase::AddNotify(const FAnimNotifyEvent& Notify)
{
	Notifies.Add(Notify);
	UE_LOG("AnimSequenceBase: Added Notify '%s' at %.2fs", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
}

void UAnimSequenceBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// Notifies 직렬화
	if (bInIsLoading)
	{
		// Load notifies from JSON
		JSON NotifiesJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Notifies", NotifiesJson, nullptr, false))
		{
			Notifies.clear();
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

		// SequenceLength, RateScale 직렬화
		FJsonSerializer::ReadFloat(InOutHandle, "SequenceLength", SequenceLength, 0.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "RateScale", RateScale, 1.0f, false);
	}
	else
	{
		// Save notifies to JSON
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
		InOutHandle["Notifies"] = NotifiesArray;

		// SequenceLength, RateScale 직렬화
		InOutHandle["SequenceLength"] = SequenceLength;
		InOutHandle["RateScale"] = RateScale;
	}
}
