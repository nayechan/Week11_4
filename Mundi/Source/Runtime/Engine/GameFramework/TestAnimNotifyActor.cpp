#include "pch.h"
#include "TestAnimNotifyActor.h"
#include "AnimSequence.h"
#include "CharacterAnimInstance.h"
#include "GlobalConsole.h"

ATestAnimNotifyActor::ATestAnimNotifyActor()
{
    // SkeletalMeshComponent 생성
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
    SkeletalMeshComponent->SetOwner(this);

    // BeginPlay 전에 미리 AnimationMode 설정 (중요!)
    // 이렇게 해야 SkeletalMeshComponent::BeginPlay()에서 자동 재생을 스킵합니다
    SkeletalMeshComponent->AnimationMode = EAnimationMode::AnimationLuaScript;
}

void ATestAnimNotifyActor::BeginPlay()
{
    Super::BeginPlay();

    // ⭐ PIE 복제 후 올바른 Component 포인터 찾기
    // PIE 시작 시 Actor와 Component 모두 복제되므로,
    // Constructor에서 설정한 포인터는 구 에디터 객체를 가리킵니다.
    // GetComponent()를 사용하여 PIE 월드의 실제 Component를 찾습니다.
    SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetComponent(USkeletalMeshComponent::StaticClass()));

    // SkeletalMesh가 설정되어 있는지 확인
    if (!SkeletalMeshComponent || !SkeletalMeshComponent->GetSkeletalMesh())
    {
        UE_LOG("[WARNING] TestAnimNotifyActor: SkeletalMesh not set! Please assign in editor.");
        return;
    }

    // 현재 재생 중인 AnimationData 가져오기 (Base Animation)
    UAnimSequence* BaseAnim = SkeletalMeshComponent->AnimationData;

    if (!BaseAnim)
    {
        UE_LOG("[WARNING] TestAnimNotifyActor: No AnimSequence assigned! Please assign animation in editor.");
        return;
    }

    UE_LOG("TestAnimNotifyActor: Found Base AnimSequence: %s", BaseAnim->GetName().c_str());
    UE_LOG("TestAnimNotifyActor: Animation Length: %.2fs", BaseAnim->SequenceLength);

    // ========================================
    // State Machine 테스트 설정
    // ========================================

    // 1. CharacterAnimInstance 생성
    UCharacterAnimInstance* CharAnimInst = NewObject<UCharacterAnimInstance>();

    // StateMachine 생성
    CharAnimInst->StateMachine = NewObject<UAnimStateMachine>();

    // 2. 3개의 State를 위한 애니메이션 생성 (테스트용으로 같은 애니메이션 복사 사용)
    // 실제로는 다른 애니메이션을 로드해야 하지만, 테스트를 위해 같은 애니메이션 사용
    UAnimSequence* IdleAnim = BaseAnim;
    UAnimSequence* WalkAnim = BaseAnim;
    UAnimSequence* RunAnim = BaseAnim;

    // 3. 각 애니메이션에 고유한 Notify 추가
    IdleAnim->AddNotify(0.5f, "Idle_Breath");
    IdleAnim->AddNotify(1.0f, "Idle_Shift");

    WalkAnim->AddNotify(0.3f, "Walk_Footstep_Left");
    WalkAnim->AddNotify(0.8f, "Walk_Footstep_Right");

    RunAnim->AddNotify(0.2f, "Run_FastStep_Left");
    RunAnim->AddNotify(0.5f, "Run_FastStep_Right");

    // 4. State Machine에 State 추가
    CharAnimInst->StateMachine->AddState("Idle", IdleAnim, true, 1.0f);
    CharAnimInst->StateMachine->AddState("Walk", WalkAnim, true, 1.0f);
    CharAnimInst->StateMachine->AddState("Run", RunAnim, true, 1.5f);  // Run은 1.5배 빠르게

    // 5. Transition 설정 (Phase 2)
    CharAnimInst->StateMachine->AddTransition("Idle", "Walk", 0.5f);  // 0.5초 블렌딩
    CharAnimInst->StateMachine->AddTransition("Walk", "Run", 0.3f);   // 0.3초 블렌딩
    CharAnimInst->StateMachine->AddTransition("Run", "Idle", 0.7f);   // 0.7초 블렌딩

    // 6. 초기 상태 설정
    CharAnimInst->StateMachine->SetInitialState("Idle");

    // 7. AnimInstance를 SkeletalMeshComponent에 설정
    SkeletalMeshComponent->SetAnimInstance(CharAnimInst);  // ⭐ OwnerComponent 자동 설정
    SkeletalMeshComponent->AnimationMode = EAnimationMode::AnimationLuaScript;  // Custom AnimInstance 사용

    UE_LOG("TestAnimNotifyActor: State Machine initialized with 3 states (Idle/Walk/Run)");
}

void ATestAnimNotifyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // AnimInstance가 설정되지 않았으면 리턴
    if (!SkeletalMeshComponent || !SkeletalMeshComponent->AnimInstance)
        return;

    UCharacterAnimInstance* CharAnimInst = Cast<UCharacterAnimInstance>(SkeletalMeshComponent->AnimInstance);
    if (!CharAnimInst || !CharAnimInst->StateMachine)
        return;

    // 경과 시간 누적
    ElapsedTime += DeltaTime;

    // 시간에 따라 State 전환
    if (ElapsedTime >= 2.0f && !bTransitionedToWalk)
    {
        UE_LOG(">>> [%.2fs] Transitioning: Idle -> Walk", ElapsedTime);
        CharAnimInst->StateMachine->TransitionTo("Walk");
        bTransitionedToWalk = true;
    }
    else if (ElapsedTime >= 5.0f && !bTransitionedToRun)
    {
        UE_LOG(">>> [%.2fs] Transitioning: Walk -> Run", ElapsedTime);
        CharAnimInst->StateMachine->TransitionTo("Run");
        bTransitionedToRun = true;
    }
    else if (ElapsedTime >= 8.0f && bTransitionedToRun)
    {
        UE_LOG(">>> [%.2fs] Transitioning: Run -> Idle (reset test)", ElapsedTime);
        CharAnimInst->StateMachine->TransitionTo("Idle");

        // 테스트 리셋 (계속 반복)
        ElapsedTime = 0.0f;
        bTransitionedToWalk = false;
        bTransitionedToRun = false;
    }
}

void ATestAnimNotifyActor::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    // State Machine 정보 가져오기
    FString CurrentStateName = "N/A";
    bool bIsTransitioning = false;
    float TransitionAlpha = 0.0f;

    if (SkeletalMeshComponent && SkeletalMeshComponent->AnimInstance)
    {
        UCharacterAnimInstance* CharAnimInst = Cast<UCharacterAnimInstance>(SkeletalMeshComponent->AnimInstance);
        if (CharAnimInst && CharAnimInst->StateMachine)
        {
            CurrentStateName = CharAnimInst->StateMachine->GetCurrentState().ToString();
            bIsTransitioning = CharAnimInst->StateMachine->IsTransitioning();
            TransitionAlpha = CharAnimInst->StateMachine->GetTransitionAlpha();
        }
    }

    // Notify 트리거 시 State Machine 정보와 함께 콘솔에 출력 (단순화된 형식)
    FString StatusStr = bIsTransitioning
        ? FString("TRANSITIONING")
        : FString("STEADY");

    UE_LOG("[NOTIFY] %s @%.2fs | State: %s (%s)",
           Notify.NotifyName.ToString().c_str(),
           Notify.TriggerTime,
           CurrentStateName.c_str(),
           StatusStr.c_str());
}
