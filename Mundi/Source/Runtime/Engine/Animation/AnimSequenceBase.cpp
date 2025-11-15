#include "pch.h"
#include "AnimSequenceBase.h"
#include "GlobalConsole.h"

void UAnimSequenceBase::GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies) const
{
	OutNotifies.Empty();

	for (const FAnimNotifyEvent& Notify : Notifies)
	{
		// 시작 시간과 끝 시간 사이에 있는 Notify만 추가
		if (Notify.TriggerTime >= StartTime && Notify.TriggerTime <= EndTime)
		{
			OutNotifies.Add(Notify);
		}
	}
}

void UAnimSequenceBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// SequenceLength, RateScale 로드
		if (InOutHandle.contains("SequenceLength"))
			SequenceLength = InOutHandle["SequenceLength"];
		if (InOutHandle.contains("RateScale"))
			RateScale = InOutHandle["RateScale"];

		// Notifies 로드
		if (InOutHandle.contains("Notifies"))
		{
			auto& NotifiesArray = InOutHandle["Notifies"];
			Notifies.Empty();
			for (auto& NotifyJson : NotifiesArray)
			{
				FAnimNotifyEvent Notify;
				if (NotifyJson.contains("TriggerTime"))
					Notify.TriggerTime = NotifyJson["TriggerTime"];
				if (NotifyJson.contains("Duration"))
					Notify.Duration = NotifyJson["Duration"];
				if (NotifyJson.contains("NotifyName"))
					Notify.NotifyName = FName(NotifyJson["NotifyName"].get<std::string>());
				if (NotifyJson.contains("NotifyData"))
					Notify.NotifyData = NotifyJson["NotifyData"].get<std::string>();
				Notifies.Add(Notify);
			}
		}
	}
	else
	{
		// SequenceLength, RateScale 저장
		InOutHandle["SequenceLength"] = SequenceLength;
		InOutHandle["RateScale"] = RateScale;

		// Notifies 저장
		JSON NotifiesArray = JSON::array();
		for (const FAnimNotifyEvent& Notify : Notifies)
		{
			JSON NotifyJson;
			NotifyJson["TriggerTime"] = Notify.TriggerTime;
			NotifyJson["Duration"] = Notify.Duration;
			NotifyJson["NotifyName"] = Notify.NotifyName.ToString();
			NotifyJson["NotifyData"] = Notify.NotifyData;
			NotifiesArray.push_back(NotifyJson);
		}
		InOutHandle["Notifies"] = NotifiesArray;
	}
}
