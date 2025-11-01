#pragma once
#include <sol/sol.hpp>
#include <sol/coroutine.hpp>

struct FLuaCoroHandle {
    uint32_t Id = 0;
    explicit operator bool() const { return Id != 0; }
};

enum class EWaitType
{
    None,
    Time,		// 시간, Wait
    Predicate,	// 조건 람다 함수
    Event		// 특정 이벤트 트리거
};

struct FCoroTask
{
    sol::thread Thread;
    sol::coroutine Co;
    void* Owner = nullptr;          // ULuaScriptComponent*
    EWaitType WaitType  = EWaitType::None;
    double WakeTime = 0.0;			// wait_time(n초)
    std::function<bool()> Predicate;// wait_until()
    std::string EventName;			// wait_event("Test")
    bool Finished = false;
    uint32 Id = 0;
};

class FLuaCoroutineScheduler
{
public:
    FLuaCoroutineScheduler();
    ~FLuaCoroutineScheduler() = default;

    FLuaCoroHandle Register(sol::thread&& InThread, sol::coroutine&& Co, void* Owner);
        
    void Tick(double DeltaTime);
    void AddCoroutine(sol::coroutine&& Co);
    void TriggerEvent(const FString& EventName);
    
    void CancelByOwner(void* Owner);
    void ShutdownBeforeLuaClose();
    
private:
    void Process(double Now);

private:
    TArray<FCoroTask> Tasks;
    uint32 NextId = 0;
    
    double NowSeconds = 0.0;
    double MaxDeltaClamp = 0.1; // 한 프레임의 최대 반영시간, Debug으로 중단 시에도 시간이 가지 않게 방지
};
