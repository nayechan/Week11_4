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


FGpuTimeQuery FGpuProfiler::GpuTimeQueries[FGpuProfiler::NUM_FRAME_BUFFERS];
int32 FGpuProfiler::CurrentFrameBuffer = 0;

void FGpuProfiler::Initialize(ID3D11Device* InDevice)
{
	// 이미 쿼리 객체가 있음
	if (GpuTimeQueries[0].DisjointQuery)
		return;
	D3D11_QUERY_DESC DisjointDesc{};
	DisjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	D3D11_QUERY_DESC TimeDesc{};
	TimeDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (int Index = 0; Index < NUM_FRAME_BUFFERS; Index++)
	{
		InDevice->CreateQuery(&DisjointDesc, GpuTimeQueries[Index].DisjointQuery.GetAddressOf());
		InDevice->CreateQuery(&TimeDesc, GpuTimeQueries[Index].StartTimeQuery.GetAddressOf());
		InDevice->CreateQuery(&TimeDesc, GpuTimeQueries[Index].EndTimeQuery.GetAddressOf());
		GpuTimeQueries[Index].bIsPending = false;
	}
}

void FGpuProfiler::BeginFrame(ID3D11DeviceContext* InDeviceContext)
{
	CurrentFrameBuffer = (CurrentFrameBuffer + 1) % NUM_FRAME_BUFFERS;

	FGpuTimeQuery& CurrentGpuTimeQuery = GpuTimeQueries[CurrentFrameBuffer];
	// 현재 프레임의 쿼리가 아직 수집이 안됨. 결과가 나왔으면 수집 후 새 프레임 시작, 아니면 포기할거임.
	if (CurrentGpuTimeQuery.bIsPending)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData;
		// D3D11_ASYNC_GETDATA_DONOTFLUSH: GPU 처리가 다 끝날때까지 기다리지 않 음(Do not FLUSH), 처리가 끝나야 S_OK 리턴
		if ((InDeviceContext->GetData(CurrentGpuTimeQuery.DisjointQuery.Get(), &DisjointData, sizeof(DisjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH)) == S_OK)
		{
			CurrentGpuTimeQuery.bIsPending = false;
			// Disjoint True : 측정 값이 손상되었다는 의미(GPU 클럭이 측정 도중 바뀌는 등)
			if (DisjointData.Disjoint == false)
			{
				UINT64 StartTime, EndTime;
				// Disjoint 쿼리 Start End 사이에 TIMESTAMP 쿼리를 진행하므로 쿼리 처리가 끝났음이 보장됨. DONOTFLUSH 플래그 필요없음
				InDeviceContext->GetData(CurrentGpuTimeQuery.StartTimeQuery.Get(), &StartTime, sizeof(UINT64), 0);
				InDeviceContext->GetData(CurrentGpuTimeQuery.EndTimeQuery.Get(), &EndTime, sizeof(UINT64), 0);

				double GpuTimeInMs = FWindowsPlatformTime::GetElapsedTimeInMs(EndTime - StartTime, DisjointData.Frequency);

				FScopeCycleCounter::AddTimeProfile(TStatId("MeshDrawTimeInGpu"), GpuTimeInMs);
			}
		}
	}

	// else가 아닌 이유: 위의 if문 안에서 쿼리 처리가 끝났을 수도 있음.
	if (!CurrentGpuTimeQuery.bIsPending)
	{
		InDeviceContext->Begin(CurrentGpuTimeQuery.DisjointQuery.Get());
	}
}

void FGpuProfiler::TimeStampStart(ID3D11DeviceContext* InDeviceContext)
{
	if (!GpuTimeQueries[CurrentFrameBuffer].bIsPending)
	{
		InDeviceContext->End(GpuTimeQueries[CurrentFrameBuffer].StartTimeQuery.Get());
	}
}

void FGpuProfiler::TimeStampEnd(ID3D11DeviceContext* InDeviceContext)
{
	if (!GpuTimeQueries[CurrentFrameBuffer].bIsPending)
	{
		InDeviceContext->End(GpuTimeQueries[CurrentFrameBuffer].EndTimeQuery.Get());
	}
}

void FGpuProfiler::EndFrame(ID3D11DeviceContext* InDeviceContext)
{
	if (!GpuTimeQueries[CurrentFrameBuffer].bIsPending)
	{
		InDeviceContext->End(GpuTimeQueries[CurrentFrameBuffer].DisjointQuery.Get());
		GpuTimeQueries[CurrentFrameBuffer].bIsPending = true;
	}
}
