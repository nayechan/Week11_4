#pragma once
#include "wrl/client.h"
// NameSpace 그대로 두고 ComPtr만 사용함
using Microsoft::WRL::ComPtr;

#define TIME_PROFILE(Key)\
FScopeCycleCounter Key##Counter(#Key); //현재 스코프 단위로 측정


#define TIME_PROFILE_END(Key)\
Key##Counter.Finish();




class FWindowsPlatformTime
{
public:
	static double GSecondsPerCycle; // 0
	static bool bInitialized; // false

	static void InitTiming()
	{
		if (!bInitialized)
		{
			bInitialized = true;

			double Frequency = (double)GetFrequency();
			if (Frequency <= 0.0)
			{
				Frequency = 1.0;
			}

			GSecondsPerCycle = 1.0 / Frequency;
		}
	}

	// ElapsedTicks: 틱 수
	// Frequency: GPU 초당 틱 수
	static double GetElapsedTimeInMs(UINT64 ElpasedTicks, UINT64 Frequency)
	{
		double Seconds = (double)ElpasedTicks / (double)Frequency;

		return Seconds * 1000.0f;
	}
	static double GetSecondsPerCycle()
	{
		if (!bInitialized)
		{
			InitTiming();
		}
		return (double)GSecondsPerCycle;
	}
	static uint64 GetFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return Frequency.QuadPart;
	}
	static double ToMilliseconds(uint64 CycleDiff)
	{
		double Ms = static_cast<double>(CycleDiff)
			* GetSecondsPerCycle()
			* 1000.0;

		return Ms;
	}

	static uint64 Cycles64()
	{
		LARGE_INTEGER CycleCount;
		QueryPerformanceCounter(&CycleCount);
		return (uint64)CycleCount.QuadPart;
	}
};

struct TStatId
{
	FString Key;
	TStatId() = default;
	TStatId(const FString& InKey) : Key(InKey) {}
};
struct FTimeProfile
{
	double Milliseconds;
	uint32 CallCount;

	const char* GetConstChar() const
	{
		static char buffer[64]; // static으로 해야 반환 가능
		sprintf_s(buffer, sizeof(buffer), " : %.3fms, Call : %d", Milliseconds, CallCount);
		return buffer;
	}

	const wchar_t* GetConstWChar_t() const
	{
		static wchar_t buffer[64];
		swprintf_s(buffer, _countof(buffer), L" : %.3fms, Call : %d", Milliseconds, CallCount);
		return buffer;
	}

	const char* GetConstCharWithKey(const FString& Key) const
	{
		static char buffer[64]; // static으로 해야 반환 가능
		sprintf_s(buffer, sizeof(buffer), "%s : %.3fms, Call : %d", Key.c_str(), Milliseconds, CallCount);
		return buffer;
	}

	const wchar_t* GetConstWChar_tWithKey(const FString& Key) const
	{
		static wchar_t buffer[64];

		swprintf_s(buffer, _countof(buffer), L"%s : %.3fms, Call : %d", std::wstring(Key.begin(), Key.end()).c_str(), Milliseconds, CallCount);
		return buffer;
	}
};

typedef FWindowsPlatformTime FPlatformTime;

class FScopeCycleCounter
{
public:
	FScopeCycleCounter(TStatId StatId)
		: StartCycles(FPlatformTime::Cycles64()) //생성 시 사이클 저장
		, UsedStatId(StatId) //키값 저장
	{
	}
	FScopeCycleCounter() : StartCycles(FPlatformTime::Cycles64()), UsedStatId()
	{
	}

	FScopeCycleCounter(const FString& Key) : StartCycles(FPlatformTime::Cycles64()), UsedStatId(TStatId(Key))
	{
	}

	~FScopeCycleCounter()
	{
		Finish(); //소멸 시 현재 사이클 구해서 현재 - 생성 사이클로 시간 측정
	}

	double Finish()
	{
		if (bIsFinish == true)
		{
			return 0;
		}
		bIsFinish = true;
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 CycleDiff = EndCycles - StartCycles;

		double Milliseconds = FWindowsPlatformTime::ToMilliseconds(CycleDiff);
		if (UsedStatId.Key.empty() == false)
		{
			AddTimeProfile(UsedStatId, Milliseconds); //키 값이 있을경우 Map에 저장
		}
		return Milliseconds;
	}

	static void AddTimeProfile(const TStatId& Key, double InMilliseconds);
	static void TimeProfileInit();

	//이거 왜 안됨?
	//static const TMap<FString, FTimeProfile>& GetTimeProfiles();

	static const TArray<FString> GetTimeProfileKeys();
	static const TArray<FTimeProfile> GetTimeProfileValues();
	static const FTimeProfile& GetTimeProfile(const FString& Key);
private:
	bool bIsFinish = false;
	uint64 StartCycles;
	TStatId UsedStatId;

};

struct FGpuTimeQuery
{
	ComPtr<ID3D11Query> DisjointQuery;
	ComPtr<ID3D11Query> StartTimeQuery;
	ComPtr<ID3D11Query> EndTimeQuery;

	bool bIsPending = false;
};

// 풀을 따로 안 만들고 일단 단일 이벤트 측정 용도로 만듦. 
class FGpuProfiler
{
public:
	static void Initialize(ID3D11Device* InDevice);
	static void BeginFrame(ID3D11DeviceContext* InDeviceContext);
	static void TimeStampStart(ID3D11DeviceContext* InDeviceContext);
	static void TimeStampEnd(ID3D11DeviceContext* InDeviceContext);
	static void EndFrame(ID3D11DeviceContext* InDeviceContext);

	// 쿼리를 여러개 만드는 이유: 매 프레임 쿼리하면 GPU가 아직 쿼리를 처리하지 못 했을때 CPU가 요구하는 경우가 많아지고
	// 프로파일링 결과가 n프레임 뒤에 업데이트되는 문제가 생김. 버퍼를 만들어서 여러번 쿼리하고 지연된 프레임을 메꿔줘야함.
	// 버퍼 늘릴수록 GPU 병목이 있어도 놓치는 프레임이 적어짐. 근데 이게 좋은 게 아님.
	// 병목을 빨리 잡아야지 병목때도 정확히 프로파일링 하는게 목적이 아님.
	static const int NUM_FRAME_BUFFERS = 3;
	static FGpuTimeQuery GpuTimeQueries[NUM_FRAME_BUFFERS];
	static int CurrentFrameBuffer;
private:
	FGpuProfiler() {};
	~FGpuProfiler() {};
};