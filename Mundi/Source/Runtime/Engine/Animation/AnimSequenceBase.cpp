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

	// TODO: Notifies 직렬화
	// TODO: SequenceLength, RateScale 직렬화
}
