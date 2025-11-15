#pragma once
#include "Actor.h"
#include "SkeletalMeshComponent.h"
#include "ATestAnimNotifyActor.generated.h"

UCLASS(DisplayName="애니메이션 Notify 테스트 액터", Description="AnimNotify 시스템을 테스트하기 위한 액터")
class ATestAnimNotifyActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ATestAnimNotifyActor();
    ~ATestAnimNotifyActor() override = default;

    // AActor override
    void BeginPlay() override;
    void Tick(float DeltaTime) override;

    // AnimNotify 핸들러 오버라이드
    void HandleAnimNotify(const FAnimNotifyEvent& Notify) override;

protected:
    UPROPERTY()
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;

    // State Machine 테스트용
    float ElapsedTime = 0.0f;
    bool bTransitionedToWalk = false;
    bool bTransitionedToRun = false;
};
