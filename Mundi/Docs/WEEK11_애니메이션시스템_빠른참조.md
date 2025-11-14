# Mundi Engine 애니메이션 시스템 빠른 참조

**작성일**: 2025-11-14
**버전**: 1.0

---

## 🎯 클래스 요약

| 클래스 | 타입 | 용도 | 파일 |
|--------|------|------|------|
| `UAnimationAsset` | 추상 | 애니메이션 에셋 베이스 | AnimationAsset.h |
| `UAnimSequenceBase` | 추상 | 재생 가능 애니메이션 베이스 | AnimSequenceBase.h |
| `UAnimSequence` | 구체 | 키프레임 애니메이션 | AnimSequence.h |
| `UAnimInstance` | 추상 | 애니메이션 재생 로직 베이스 | AnimInstance.h |
| `UAnimSingleNodeInstance` | 구체 | 단일 애니메이션 재생 | AnimSingleNodeInstance.h |
| `FAnimationRuntime` | 정적 | 블렌딩 유틸리티 | AnimationRuntime.h |

---

## 📦 데이터 구조

| 구조체 | 용도 | 주요 멤버 |
|--------|------|-----------|
| `FFrameRate` | 프레임 레이트 | `Numerator`, `Denominator` |
| `FRawAnimSequenceTrack` | 키프레임 데이터 | `PosKeys`, `RotKeys`, `ScaleKeys` |
| `FBoneAnimationTrack` | 본-키프레임 연결 | `Name`, `BoneTreeIndex`, `InternalTrack` |
| `FAnimNotifyEvent` | 애니메이션 이벤트 | `TriggerTime`, `NotifyName` |
| `FAnimExtractContext` | 포즈 추출 설정 | `CurrentTime`, `bLooping` |
| `FPoseContext` | 전체 포즈 스냅샷 | `BoneTransforms[]` |

---

## 🚀 빠른 시작

### 1. 애니메이션 재생 (C++)

```cpp
// 컴포넌트 가져오기
USkeletalMeshComponent* SkelMesh = Actor->GetComponent<USkeletalMeshComponent>();

// 애니메이션 로드
UAnimSequence* WalkAnim = LoadAnimation("Character/Walk.anim");

// 재생 (루핑)
SkelMesh->PlayAnimation(WalkAnim, true);

// 정지
SkelMesh->StopAnimation();
```

### 2. 애니메이션 재생 (Lua)

```lua
-- 컴포넌트 가져오기
local skelMesh = actor:GetComponent("USkeletalMeshComponent")

-- 애니메이션 재생 (루핑)
skelMesh:애니메이션_재생(walkAnim, true)

-- 애니메이션 정지
skelMesh:애니메이션_정지()
```

### 3. 포즈 추출

```cpp
// 컨텍스트 생성 (1.5초 시점, 비루핑)
FAnimExtractContext Context(1.5f, false);

// 포즈 추출
FPoseContext Pose;
AnimSequence->GetAnimationPose(Pose, Context);

// 특정 본의 트랜스폼 접근
FTransform RootTransform = Pose.BoneTransforms[0];
```

### 4. 포즈 블렌딩

```cpp
// 두 포즈 추출
FPoseContext WalkPose, RunPose, BlendedPose;
WalkAnim->GetAnimationPose(WalkPose, Context);
RunAnim->GetAnimationPose(RunPose, Context);

// 50% 블렌딩
FAnimationRuntime::BlendTwoPosesTogether(WalkPose, RunPose, 0.5f, BlendedPose);
```

---

## 🔧 주요 메서드

### UAnimSequence

```cpp
// 포즈 추출
void GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context);

// 특정 본의 트랜스폼 계산 (보간 포함)
FTransform GetBoneTransformAtTime(int32 BoneIndex, float Time) const;

// 본 트랙 추가 (FBX Loader 사용)
void AddBoneTrack(const FBoneAnimationTrack& Track);

// 모든 본 트랙 가져오기
const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;

// 애니메이션 길이 반환
float GetPlayLength() const;  // SequenceLength 반환
```

### UAnimSingleNodeInstance

```cpp
// 애니메이션 설정
void SetAnimationAsset(UAnimSequence* NewAsset);

// 재생 제어
void Play(bool bInLooping);
void Stop();
void Pause();
void SetPlayRate(float InPlayRate);  // 1.0 = 정상 속도

// 상태 확인
bool IsPlaying() const;
bool IsLooping() const;
float GetPlayRate() const;
```

### USkeletalMeshComponent

```cpp
// 애니메이션 재생 (Lua 바인딩)
void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping);

// 애니메이션 정지 (Lua 바인딩)
void StopAnimation();

// 애니메이션 모드 설정
void SetAnimationMode(EAnimationMode InMode);

// AnimNotify 핸들링
void HandleAnimNotify(const FAnimNotifyEvent& Notify);
```

### FAnimationRuntime

```cpp
// 두 포즈 블렌딩
static void BlendTwoPosesTogether(
    const FPoseContext& PoseA,
    const FPoseContext& PoseB,
    float BlendAlpha,        // 0.0 ~ 1.0
    FPoseContext& OutPose
);

// 두 트랜스폼 블렌딩
static FTransform BlendTransforms(
    const FTransform& A,
    const FTransform& B,
    float Alpha              // 0.0 ~ 1.0
);
```

---

## 💡 자주 사용하는 패턴

### 패턴 1: 더미 애니메이션 생성 (테스트용)

```cpp
UAnimSequence* CreateTestAnimation()
{
    UAnimSequence* Anim = NewObject<UAnimSequence>();

    // 메타데이터
    Anim->FrameRate = {30, 1};
    Anim->NumberOfFrames = 90;
    Anim->SequenceLength = 3.0f;

    // 루트 본 트랙
    FBoneAnimationTrack RootTrack(FName("Root"), 0);

    // 키프레임 추가 (360도 회전)
    for (int i = 0; i < 90; ++i)
    {
        float Angle = (i / 90.0f) * 360.0f;

        RootTrack.InternalTrack.PosKeys.Add(FVector(0, 0, 0));
        RootTrack.InternalTrack.RotKeys.Add(
            FQuat::FromAxisAngle(FVector::UpVector, FMath::DegreesToRadians(Angle))
        );
        RootTrack.InternalTrack.ScaleKeys.Add(FVector(1, 1, 1));
    }

    Anim->AddBoneTrack(RootTrack);
    return Anim;
}
```

### 패턴 2: AnimNotify 추가 및 핸들링

```cpp
// Notify 추가
FAnimNotifyEvent Footstep(0.3f, FName("Footstep"));
Footstep.NotifyData = "{\"volume\": 0.8}";
WalkAnim->Notifies.Add(Footstep);

// Notify 핸들링
void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    if (Notify.NotifyName == FName("Footstep"))
    {
        PlaySound("Footstep.wav");
    }
    else if (Notify.NotifyName == FName("Jump"))
    {
        SpawnParticle("Jump.vfx");
    }
}
```

### 패턴 3: 속도 기반 블렌딩

```cpp
void BlendIdleToWalk(float Speed)
{
    // 포즈 추출
    FPoseContext IdlePose, WalkPose, BlendedPose;
    IdleAnim->GetAnimationPose(IdlePose, Context);
    WalkAnim->GetAnimationPose(WalkPose, Context);

    // 속도에 따라 블렌딩 비율 계산
    const float MaxWalkSpeed = 400.0f;
    float BlendAlpha = FMath::Clamp(Speed / MaxWalkSpeed, 0.0f, 1.0f);

    // 블렌딩
    FAnimationRuntime::BlendTwoPosesTogether(IdlePose, WalkPose, BlendAlpha, BlendedPose);

    // 적용
    ApplyPoseToSkeleton(BlendedPose);
}
```

### 패턴 4: State Machine 기본 구조

```cpp
class UCharacterAnimInstance : public UAnimInstance
{
public:
    enum class EState { Idle, Walk, Run, Jump };

    virtual void NativeUpdateAnimation(float DeltaSeconds) override
    {
        Super::NativeUpdateAnimation(DeltaSeconds);

        // 상태 업데이트
        UpdateState();

        // 포즈 블렌딩
        BlendStates(DeltaSeconds);
    }

private:
    EState CurrentState = EState::Idle;
    EState PreviousState = EState::Idle;
    float TransitionTime = 0.0f;

    void UpdateState()
    {
        float Speed = GetOwnerComponent()->GetOwner()->GetVelocity().Size();

        if (Speed < 10.0f)
            TransitionTo(EState::Idle);
        else if (Speed < 300.0f)
            TransitionTo(EState::Walk);
        else
            TransitionTo(EState::Run);
    }

    void TransitionTo(EState NewState)
    {
        if (CurrentState != NewState)
        {
            PreviousState = CurrentState;
            CurrentState = NewState;
            TransitionTime = 0.0f;
        }
    }

    void BlendStates(float DeltaSeconds)
    {
        const float TransitionDuration = 0.3f;
        TransitionTime += DeltaSeconds;

        if (TransitionTime < TransitionDuration)
        {
            // 전환 중: 블렌딩
            float Alpha = TransitionTime / TransitionDuration;

            FPoseContext PrevPose, CurrPose, BlendedPose;
            GetStateAnimation(PreviousState)->GetAnimationPose(PrevPose, Context);
            GetStateAnimation(CurrentState)->GetAnimationPose(CurrPose, Context);

            FAnimationRuntime::BlendTwoPosesTogether(PrevPose, CurrPose, Alpha, BlendedPose);
            ApplyPose(BlendedPose);
        }
        else
        {
            // 전환 완료: 현재 상태만
            FPoseContext Pose;
            GetStateAnimation(CurrentState)->GetAnimationPose(Pose, Context);
            ApplyPose(Pose);
        }
    }
};
```

---

## 🎨 보간 알고리즘

### 위치/스케일 보간 (선형)

```cpp
FVector Lerp(const FVector& A, const FVector& B, float Alpha)
{
    return A + (B - A) * Alpha;
}
```

### 회전 보간 (Slerp)

```cpp
FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha)
{
    // 구 표면의 최단 경로로 보간
    // 자세한 구현은 FQuat::Slerp() 참조
}
```

### 프레임 인덱스 계산

```cpp
// 시간(초) → 프레임 번호 변환
float Time = 1.5f;  // 1.5초
float FrameTime = Time * FrameRate.AsDecimal();  // 1.5 * 30 = 45.0

int32 Frame0 = (int32)FrameTime;      // 45
int32 Frame1 = Frame0 + 1;            // 46
float Alpha = FrameTime - Frame0;     // 0.0 (소수 부분)

// 보간
FVector Position = Lerp(Keys[Frame0], Keys[Frame1], Alpha);
```

---

## 🔍 문제 해결

### 애니메이션이 재생되지 않음

**체크리스트**:
- [ ] `USkeletalMeshComponent::AnimInstance`가 null이 아닌가?
- [ ] `UAnimSingleNodeInstance::CurrentSequence`가 설정되었나?
- [ ] `bIsPlaying`이 true인가?
- [ ] `TickAnimation()`이 호출되고 있나?
- [ ] 애니메이션 길이가 0이 아닌가?

```cpp
// 디버그 로그 추가
void TickAnimation(float DeltaTime)
{
    if (!AnimInstance)
    {
        UE_LOG("TickAnimation: AnimInstance is null!");
        return;
    }

    UE_LOG("TickAnimation: CurrentTime=%.2f, DeltaTime=%.2f",
           AnimInstance->GetCurrentTime(), DeltaTime);
}
```

### 포즈가 이상하게 보임

**원인**:
- 본 인덱스 불일치
- 키프레임 데이터 부족
- 보간 오류

```cpp
// 포즈 검증
void ValidatePose(const FPoseContext& Pose)
{
    UE_LOG("Pose has %d bones", Pose.GetNumBones());

    for (int i = 0; i < Pose.GetNumBones(); ++i)
    {
        const FTransform& T = Pose.BoneTransforms[i];
        UE_LOG("Bone %d: Pos=(%.2f,%.2f,%.2f)",
               i, T.Translation.X, T.Translation.Y, T.Translation.Z);
    }
}
```

### 블렌딩이 부드럽지 않음

**해결책**:
- BlendAlpha를 부드럽게 변경 (스무스 스텝 사용)
- 전환 시간 늘리기
- Slerp 대신 NLerp 사용 (빠르지만 덜 정확)

```cpp
// 스무스 스텝 함수
float SmoothStep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// 블렌딩 시 사용
float RawAlpha = TransitionTime / TransitionDuration;
float SmoothAlpha = SmoothStep(RawAlpha);
FAnimationRuntime::BlendTwoPosesTogether(PoseA, PoseB, SmoothAlpha, OutPose);
```

---

## 📊 성능 팁

### 1. LOD (Level of Detail)

```cpp
// 거리에 따라 업데이트 주기 조절
float Distance = GetDistanceToCamera();

if (Distance < 500.0f)
{
    // 가까움: 매 프레임 업데이트
    TickAnimation(DeltaTime);
}
else if (Distance < 1000.0f)
{
    // 중간: 2프레임마다 업데이트
    if (FrameCount % 2 == 0)
        TickAnimation(DeltaTime * 2.0f);
}
else
{
    // 멀리: 5프레임마다 업데이트
    if (FrameCount % 5 == 0)
        TickAnimation(DeltaTime * 5.0f);
}
```

### 2. 키프레임 압축

```cpp
// 정적인 본의 키프레임 제거
void RemoveRedundantKeys(FRawAnimSequenceTrack& Track, float Tolerance = 0.001f)
{
    // 위치가 거의 변하지 않으면 키프레임 1개만 유지
    if (AreAllKeysNearlyEqual(Track.PosKeys, Tolerance))
    {
        FVector AvgPos = AverageKeys(Track.PosKeys);
        Track.PosKeys.Empty();
        Track.PosKeys.Add(AvgPos);
    }

    // 회전, 스케일도 동일하게 처리
}
```

### 3. 포즈 캐싱

```cpp
// 자주 사용되는 포즈 캐싱
class FPoseCache
{
    TMap<float, FPoseContext> CachedPoses;

    FPoseContext* GetCachedPose(float Time, float CacheInterval = 0.1f)
    {
        // 0.1초 간격으로 캐싱
        float CacheKey = FMath::RoundToFloat(Time / CacheInterval) * CacheInterval;

        if (FPoseContext* Cached = CachedPoses.Find(CacheKey))
            return Cached;

        return nullptr;
    }
};
```

---

## 📚 추가 자료

### 관련 파일

- [AnimationTypes.h](Source/Runtime/Engine/Animation/AnimationTypes.h) - 데이터 구조
- [AnimSequence.h](Source/Runtime/Engine/Animation/AnimSequence.h) - 키프레임 애니메이션
- [AnimInstance.h](Source/Runtime/Engine/Animation/AnimInstance.h) - 애니메이션 인스턴스
- [AnimationRuntime.h](Source/Runtime/Engine/Animation/AnimationRuntime.h) - 블렌딩 유틸리티

### 관련 문서

- [애니메이션시스템_API문서.md](WEEK11_애니메이션시스템_API문서.md) - 상세 API 문서
- [애니메이션시스템_클래스다이어그램.md](WEEK11_애니메이션시스템_클래스다이어그램.md) - 클래스 다이어그램
- [팀원1_애니메이션클래스구조_구현계획.md](WEEK11_팀원1_애니메이션클래스구조_구현계획.md) - 구현 계획

### 외부 참고

- 언리얼 엔진 애니메이션: https://docs.unrealengine.com/en-US/AnimatingObjects/
- Slerp 설명: https://en.wikipedia.org/wiki/Slerp
- 키프레임 애니메이션: https://en.wikipedia.org/wiki/Key_frame

---

## 🎯 체크리스트

### 애니메이션 재생

- [ ] UAnimSequence 생성 또는 로드
- [ ] USkeletalMeshComponent 가져오기
- [ ] PlayAnimation() 호출
- [ ] TickComponent에서 TickAnimation() 호출 확인

### 커스텀 AnimInstance 구현

- [ ] UAnimInstance 상속
- [ ] NativeUpdateAnimation() 오버라이드
- [ ] State Machine 로직 구현
- [ ] FAnimationRuntime::BlendTwoPosesTogether() 사용

### AnimNotify 구현

- [ ] FAnimNotifyEvent 생성
- [ ] UAnimSequenceBase::Notifies에 추가
- [ ] HandleAnimNotify() 구현
- [ ] TriggerAnimNotifies() 호출 확인

### 블렌딩 구현

- [ ] 두 포즈 추출
- [ ] BlendAlpha 계산
- [ ] FAnimationRuntime::BlendTwoPosesTogether() 호출
- [ ] 블렌딩된 포즈 적용

---

**문서 작성자**: Claude Code
**최종 업데이트**: 2025-11-14
**버전**: 1.0
