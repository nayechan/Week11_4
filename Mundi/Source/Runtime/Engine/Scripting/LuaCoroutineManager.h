#pragma once
#include <sol/sol.hpp>
#include <sol/coroutine.hpp>

enum class EWaitType
{
    None,
    Time,		// 시간, Wait
    Predicate,	// 조건 람다 함수
    Event		// 특정 이벤트 트리거
};

struct FCoroTask
{
    sol::coroutine Co;
    EWaitType WaitType;
    double WakeTime = 0.0;			// wait(n초)
    std::function<bool()> Predicate;// wait_until()
    std::string EventName;			// wait_event("Test")
    bool Finished = false;
};

class FCoroTaskManager
{
public:
    FCoroTaskManager() = default;
    ~FCoroTaskManager() = default;
    void ShutdownBeforeLuaClose();
    
    void Tick(double DeltaTime);
    void AddCoroutine(sol::coroutine&& Co);
    void TriggerEvent(const FString& EventName);
    
private:
    void Process(double Now);

private:
    TArray<FCoroTask> Tasks;
    double NowSeconds = 0.0;
    double MaxDeltaClamp = 0.1; // 한 프레임의 최대 반영시간, Debug으로 중단 시에도 시간이 가지 않게 방지
};
