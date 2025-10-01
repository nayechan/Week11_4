#pragma once
#include "Object.h"

class AActor;

class UActorComponent : public UObject
{
public:
    DECLARE_CLASS(UActorComponent, UObject)
    UActorComponent();

protected:
    ~UActorComponent() override;

public:
    // ───────────────
    // Lifecycle
    // ───────────────
    virtual void InitializeComponent();   // 액터에 붙을 때
    virtual void BeginPlay();             // 월드 시작 시
    virtual void TickComponent(float DeltaTime); // 매 프레임
    virtual void EndPlay();               // 파괴/종료 시

    // ───────────────
    // 활성화/비활성
    // ───────────────
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }
    bool IsActive() const { return bIsActive; }

    void SetTickEnabled(bool bNewTick) { bCanEverTick = bNewTick; }
    bool CanEverTick() const { return bCanEverTick; }

    // ───────────────
    // Owner Actor
    // ───────────────
    void SetOwner(AActor* InOwner) {
        Owner = InOwner;
    }
    AActor* GetOwner() const { return Owner; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UActorComponent)

protected:
    AActor* Owner = nullptr;  // 자신을 보유한 액터
    bool bIsActive = true;    // 활성 상태
    bool bCanEverTick = false; // 매 프레임 Tick 가능 여부
};