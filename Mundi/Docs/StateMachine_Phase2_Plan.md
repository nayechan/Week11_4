# Animation State Machine - Phase 2 확장 계획

## 현재 구현 (Phase 1: 완료)

### 방식 A: BlendIn/BlendOut 기반 자유 전환

```cpp
// 특징
- 모든 State 간 자유롭게 전환 가능
- 블렌드 시간은 State별 BlendIn/Out 평균값 사용
- 간단하고 직관적
- 대부분의 게임에 충분

// 구조
struct FAnimState
{
    float BlendInTime = 0.3f;
    float BlendOutTime = 0.3f;
};

void TransitionTo(FName NewState)
{
    float BlendDuration = (From->BlendOutTime + To->BlendInTime) * 0.5f;
    // 전환 시작
}
```

---

## Phase 2: Transition 배열 방식으로 확장

### 확장 시기
- Death 상태처럼 특정 전환을 금지해야 할 때
- 전환별 블렌드 시간을 정밀하게 제어해야 할 때
- 조건부 자동 전환이 많이 필요할 때

### 추가할 구조

```cpp
// AnimStateMachine.h에 추가
struct FAnimTransition
{
    FName FromState;
    FName ToState;
    float BlendDuration = 0.3f;
    TFunction<bool()> Condition;  // 조건부 전환용
};

class UAnimStateMachine
{
    // 기존 멤버 유지
    TMap<FName, FAnimState> States;

    // 추가
    TArray<FAnimTransition> Transitions;
};
```

### 추가할 API

```cpp
// 전환 규칙 추가
void AddTransition(FName From, FName To, float BlendDuration = 0.3f);
void AddTransitionWithCondition(FName From, FName To, float Blend, TFunction<bool()> Cond);

// 전환 규칙 찾기
FAnimTransition* FindTransition(FName From, FName To);

// 자동 전환 체크
void CheckAutoTransitions();
```

### 수정할 함수

#### `TransitionTo()` 수정

```cpp
void UAnimStateMachine::TransitionTo(FName NewState)
{
    if (!States.Contains(NewState) || CurrentState == NewState)
        return;

    // 1. Transition 배열에서 전환 규칙 찾기
    FAnimTransition* Trans = FindTransition(CurrentState, NewState);

    if (!Trans)
    {
        // 허용되지 않은 전환
        UE_LOG("Transition %s -> %s not allowed!",
               CurrentState.ToString().c_str(),
               NewState.ToString().c_str());
        return;
    }

    // 2. Transition의 블렌드 시간 사용
    float BlendDuration = Trans->BlendDuration;

    StartTransition(CurrentState, NewState, BlendDuration);
}
```

#### `Update()` 수정

```cpp
void UAnimStateMachine::Update(float DeltaTime)
{
    CurrentTime += DeltaTime;

    // 1. ProcessState 호출 (수동 전환)
    ProcessState();

    // 2. 활성 전환 진행
    UpdateTransition(DeltaTime);

    // 3. 자동 전환 체크 (조건 기반)
    if (!bIsTransitioning)
    {
        CheckAutoTransitions();
    }
}
```

### 새로운 함수 구현

```cpp
void UAnimStateMachine::AddTransition(FName From, FName To, float BlendDuration)
{
    FAnimTransition Trans;
    Trans.FromState = From;
    Trans.ToState = To;
    Trans.BlendDuration = BlendDuration;
    Transitions.Add(Trans);
}

void UAnimStateMachine::AddTransitionWithCondition(
    FName From, FName To, float Blend, TFunction<bool()> Cond)
{
    FAnimTransition Trans;
    Trans.FromState = From;
    Trans.ToState = To;
    Trans.BlendDuration = Blend;
    Trans.Condition = Cond;
    Transitions.Add(Trans);
}

FAnimTransition* UAnimStateMachine::FindTransition(FName From, FName To)
{
    for (auto& Trans : Transitions)
    {
        if (Trans.FromState == From && Trans.ToState == To)
            return &Trans;
    }
    return nullptr;
}

void UAnimStateMachine::CheckAutoTransitions()
{
    for (auto& Trans : Transitions)
    {
        if (Trans.FromState == CurrentState && Trans.Condition)
        {
            if (Trans.Condition())
            {
                TransitionTo(Trans.ToState);
                break;  // 첫 번째 만족하는 전환만
            }
        }
    }
}
```

---

## 하이브리드 방식 (권장)

Phase 2에서도 기존 BlendIn/Out을 Fallback으로 유지:

```cpp
void UAnimStateMachine::TransitionTo(FName NewState)
{
    if (!States.Contains(NewState) || CurrentState == NewState)
        return;

    float BlendDuration = 0.3f;  // 기본값

    // 1. 먼저 Transition 배열에서 찾기
    FAnimTransition* Trans = FindTransition(CurrentState, NewState);
    if (Trans)
    {
        BlendDuration = Trans->BlendDuration;
    }
    else
    {
        // 2. Fallback: State의 BlendIn/Out 사용 (Phase 1 방식)
        const FAnimState* FromState = States.Find(CurrentState);
        const FAnimState* ToState = States.Find(NewState);
        if (FromState && ToState)
        {
            BlendDuration = (FromState->BlendOutTime + ToState->BlendInTime) * 0.5f;
        }
    }

    StartTransition(CurrentState, NewState, BlendDuration);
}
```

**장점:**
- 명시적 Transition이 없어도 동작 (Phase 1과 호환)
- 필요한 경우만 Transition 정의
- 점진적 마이그레이션 가능

---

## 사용 예시

### Phase 1 (현재)

```cpp
// 간단한 설정
sm->AddState("Idle", IdleAnim);
sm->AddState("Walk", WalkAnim);
sm->AddState("Attack", AttackAnim);

// 자유 전환
sm->TransitionTo("Attack");  // 어디서든 가능
```

### Phase 2

```cpp
// 상태 정의 (동일)
sm->AddState("Idle", IdleAnim);
sm->AddState("Walk", WalkAnim);
sm->AddState("Attack", AttackAnim);
sm->AddState("Death", DeathAnim);

// 전환 규칙 정의
sm->AddTransition("Idle", "Walk", 0.2f);
sm->AddTransition("Walk", "Idle", 0.3f);
sm->AddTransition("Idle", "Attack", 0.1f);  // 빠른 공격
sm->AddTransition("Attack", "Idle", 0.4f);

// Death는 들어가는 전환만 정의
sm->AddTransition("Idle", "Death", 0.5f);
sm->AddTransition("Walk", "Death", 0.5f);
sm->AddTransition("Attack", "Death", 0.5f);
// Death -> X 전환 없음 (불가능)

// 조건부 자동 전환
sm->AddTransitionWithCondition("Idle", "Walk", 0.2f, [this]() {
    return Speed > 10.0f;
});

sm->AddTransitionWithCondition("Walk", "Run", 0.15f, [this]() {
    return Speed > 300.0f;
});
```

---

## 예상 소요 시간

- FAnimTransition 구조 정의: 10분
- AddTransition API 구현: 30분
- FindTransition 구현: 20분
- TransitionTo 수정 (하이브리드): 30분
- CheckAutoTransitions 구현: 40분
- 테스트 및 디버깅: 1시간

**총 예상 시간: 2.5 - 3시간**

---

## 마이그레이션 가이드

### 기존 코드 (Phase 1)

```cpp
sm->AddState("Idle", IdleAnim);
sm->AddState("Walk", WalkAnim);

void ProcessState()
{
    if (Speed > 10.0f)
        sm->TransitionTo("Walk");
    else
        sm->TransitionTo("Idle");
}
```

### Phase 2로 마이그레이션

```cpp
// 옵션 1: 기존 방식 유지 (그대로 작동)
sm->AddState("Idle", IdleAnim);
sm->AddState("Walk", WalkAnim);
// Transition 정의 안 해도 작동 (Fallback)

// 옵션 2: 조건부 자동 전환으로 변경
sm->AddState("Idle", IdleAnim);
sm->AddState("Walk", WalkAnim);

sm->AddTransitionWithCondition("Idle", "Walk", 0.2f, [this]() {
    return Speed > 10.0f;
});

sm->AddTransitionWithCondition("Walk", "Idle", 0.3f, [this]() {
    return Speed <= 10.0f;
});

// ProcessState() 구현 불필요 (자동 전환)
```

---

## 결론

**Phase 1로 시작, 필요시 Phase 2로 확장**

- Phase 1: 80%의 게임에 충분, 2-3시간 구현
- Phase 2: 20%의 복잡한 경우, 추가 3시간 구현
- 하이브리드 방식: 둘 다 지원, 점진적 확장 가능
