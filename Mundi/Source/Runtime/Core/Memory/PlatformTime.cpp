
#include "pch.h"
#include "PlatformTime.h"

TMap<FString, FTimeProfile> TimeProfileMap;
//Map에 이미 있으면 시간, 콜스택 추가
void FScopeCycleCounter::AddTimeProfile(const TStatId& Key, double InMilliseconds)
{
	if (TimeProfileMap.Contains(Key.Key) == false)
	{
		TimeProfileMap[Key.Key] = FTimeProfile{ InMilliseconds, 1 };
	}
	else
	{
		TimeProfileMap[Key.Key].Milliseconds += InMilliseconds;
		TimeProfileMap[Key.Key].CallCount++;
	}
}
//시간, 콜스택 초기화
void FScopeCycleCounter::TimeProfileInit()
{
	const TArray<FString> Keys = TimeProfileMap.GetKeys();
	for (const FString& Key : Keys)
	{
		TimeProfileMap[Key].Milliseconds = 0;
		TimeProfileMap[Key].CallCount = 0;
	}
}
//const TMap<FString, FTimeProfile>& FScopeCycleCounter::GetTimeProfiles()
//{
//    return TimeProfileMap;
//}
const TArray<FString> FScopeCycleCounter::GetTimeProfileKeys()
{
	return TimeProfileMap.GetKeys();
}
const TArray<FTimeProfile> FScopeCycleCounter::GetTimeProfileValues()
{
	return TimeProfileMap.GetValues();
}
const FTimeProfile& FScopeCycleCounter::GetTimeProfile(const FString& Key)
{
	return TimeProfileMap[Key];
}
double FWindowsPlatformTime::GSecondsPerCycle = 0.0;
bool FWindowsPlatformTime::bInitialized = false;
