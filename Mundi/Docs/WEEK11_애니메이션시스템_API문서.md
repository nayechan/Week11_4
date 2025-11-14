# Mundi Engine 애니메이션 시스템 API 문서

**작성일**: 2025-11-14
**버전**: 1.0
**상태**: 구현 완료 (FBX 임포트 제외)

---

## 📋 목차

1. [개요](#개요)
2. [클래스 계층 구조](#클래스-계층-구조)
3. [에셋 클래스](#에셋-클래스)
4. [런타임 클래스](#런타임-클래스)
5. [데이터 구조](#데이터-구조)
6. [컴포넌트 통합](#컴포넌트-통합)
7. [유틸리티 클래스](#유틸리티-클래스)
8. [사용 가이드](#사용-가이드)
9. [API 레퍼런스](#api-레퍼런스)

---

## 개요

Mundi Engine의 애니메이션 시스템은 언리얼 엔진의 애니메이션 아키텍처를 참고하여 설계되었습니다. 키프레임 기반 애니메이션, 포즈 블렌딩, AnimNotify 시스템을 지원하며, 확장 가능한 구조로 설계되었습니다.

### 주요 특징

- **에셋/런타임 분리**: 불변 애니메이션 데이터와 가변 재생 상태 분리
- **계층적 클래스 구조**: 추상 클래스를 통한 확장 가능한 설계
- **프레임 기반 보간**: 선형 보간(Lerp) 및 구면 선형 보간(Slerp) 지원
- **AnimNotify 시스템**: 애니메이션 이벤트 트리거 지원
- **포즈 블렌딩**: 두 애니메이션 간 부드러운 전환
- **Lua 스크립팅**: 애니메이션 재생 제어를 Lua에서 가능

### 파일 위치

```
Source/Runtime/Engine/Animation/
├── AnimationTypes.h          # 데이터 구조 및 열거형
├── AnimationAsset.h/.cpp     # 애니메이션 에셋 베이스
├── AnimSequenceBase.h/.cpp   # 재생 가능한 애니메이션 베이스
├── AnimSequence.h/.cpp       # 키프레임 애니메이션
├── AnimInstance.h/.cpp       # 애니메이션 인스턴스 베이스
├── AnimSingleNodeInstance.h/.cpp  # 단일 애니메이션 재생기
└── AnimationRuntime.h/.cpp   # 블렌딩 유틸리티
```

---

## 클래스 계층 구조

### 에셋 계층 (Asset Hierarchy)

```
UResourceBase
└── UAnimationAsset (추상)
    └── UAnimSequenceBase (추상)
        └── UAnimSequence (구체)
```

- **UResourceBase**: 모든 엔진 리소스의 베이스 클래스
- **UAnimationAsset**: 애니메이션 에셋 공통 기능 (스켈레톤 참조)
- **UAnimSequenceBase**: 재생 가능한 애니메이션 (Notify, 재생 길이)
- **UAnimSequence**: 키프레임 기반 애니메이션 (실제 구현)

### 런타임 계층 (Runtime Hierarchy)

```
UObject
└── UAnimInstance (추상)
    └── UAnimSingleNodeInstance (구체)
```

- **UObject**: 모든 엔진 오브젝트의 베이스 클래스
- **UAnimInstance**: 애니메이션 재생 로직 베이스 (State Machine 확장 가능)
- **UAnimSingleNodeInstance**: 단일 애니메이션 재생 구현

### 컴포넌트 계층

```
USkinnedMeshComponent
└── USkeletalMeshComponent
```

- **USkinnedMeshComponent**: 스키닝 메쉬 렌더링 베이스
- **USkeletalMeshComponent**: 애니메이션 재생 및 본 트랜스폼 관리

---

## 에셋 클래스

### UAnimationAsset

**파일**: `Source/Runtime/Engine/Animation/AnimationAsset.h`

**설명**: 모든 애니메이션 에셋의 최상위 베이스 클래스입니다.

#### 주요 멤버

```cpp
UPROPERTY(EditAnywhere, Category="[애니메이션]")
USkeleton* Skeleton;  // 대상 스켈레톤 (필수)

UPROPERTY(EditAnywhere, Category="[애니메이션]")
TArray<UAnimMetaData*> MetaData;  // 메타데이터 배열
```

#### 주요 메서드

```cpp
// 애니메이션 길이 반환 (순수 가상)
virtual float GetPlayLength() const { return 0.0f; }

// 직렬화
virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
```

#### 사용 예시

```cpp
// 추상 클래스이므로 직접 인스턴스화 불가
// 하위 클래스 (UAnimSequence 등)를 사용
```

---

### UAnimSequenceBase

**파일**: `Source/Runtime/Engine/Animation/AnimSequenceBase.h`

**설명**: 재생 가능한 애니메이션의 베이스 클래스입니다. Notify 시스템과 재생 제어를 제공합니다.

#### 주요 멤버

```cpp
UPROPERTY(EditAnywhere, Category="[애니메이션|Notify]")
TArray<FAnimNotifyEvent> Notifies;  // 애니메이션 이벤트 배열

UPROPERTY(EditAnywhere, Category="[애니메이션]")
float SequenceLength;  // 애니메이션 길이 (초)

UPROPERTY(EditAnywhere, Category="[애니메이션]", Range="0.1, 10.0")
float RateScale;  // 재생 속도 배율
```

#### 주요 메서드

```cpp
// 포즈 추출 (순수 가상 - 하위 클래스 구현 필수)
virtual void GetAnimationPose(FPoseContext& OutPose,
                               const FAnimExtractContext& Context) = 0;

// 시간 범위 내의 Notify 가져오기
void GetAnimNotifiesInRange(float StartTime, float EndTime,
                             TArray<FAnimNotifyEvent>& OutNotifies) const;

// 애니메이션 길이 반환
virtual float GetPlayLength() const override { return SequenceLength; }
```

#### 사용 예시

```cpp
// 특정 시간 범위의 Notify 검색
TArray<FAnimNotifyEvent> Events;
AnimSeqBase->GetAnimNotifiesInRange(0.5f, 1.0f, Events);

for (const FAnimNotifyEvent& Event : Events)
{
    UE_LOG("Notify: %s at %.2f", Event.NotifyName.ToString().c_str(), Event.TriggerTime);
}
```

---

### UAnimSequence

**파일**: `Source/Runtime/Engine/Animation/AnimSequence.h`

**설명**: 키프레임 기반 애니메이션 데이터를 저장하고 보간하는 구체 클래스입니다.

#### 주요 멤버

```cpp
UPROPERTY(EditAnywhere, Category="[애니메이션]")
FFrameRate FrameRate;  // 프레임 레이트 (기본: 30fps)

UPROPERTY(EditAnywhere, Category="[애니메이션]")
int32 NumberOfFrames;  // 총 프레임 수

UPROPERTY(EditAnywhere, Category="[애니메이션]")
int32 NumberOfKeys;  // 총 키 개수

private:
TArray<FBoneAnimationTrack> BoneAnimationTracks;  // 본별 애니메이션 데이터
```

#### 주요 메서드

```cpp
// 포즈 추출 구현
virtual void GetAnimationPose(FPoseContext& OutPose,
                               const FAnimExtractContext& Context) override;

// 특정 본의 특정 시간 트랜스폼 계산 (보간 포함)
FTransform GetBoneTransformAtTime(int32 BoneIndex, float Time) const;

// 본 애니메이션 트랙 접근
const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;

// 본 트랙 추가 (FBX Loader 사용)
void AddBoneTrack(const FBoneAnimationTrack& Track);
void SetBoneTracks(const TArray<FBoneAnimationTrack>& Tracks);
```

#### 보간 알고리즘

##### 위치 보간 (선형 보간)

```cpp
FVector InterpolatePosition(const TArray<FVector>& Keys, float Time) const
{
    // 프레임 인덱스 계산
    const float FrameTime = Time * FrameRate.AsDecimal();  // 예: 1.5초 * 30fps = 45.0
    const int32 Frame0 = FMath::Clamp((int32)FrameTime, 0, Keys.Num() - 1);
    const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
    const float Alpha = FMath::Frac(FrameTime);  // 소수 부분 (0.0)

    // 선형 보간
    return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}
```

##### 회전 보간 (구면 선형 보간)

```cpp
FQuat InterpolateRotation(const TArray<FQuat>& Keys, float Time) const
{
    // 프레임 인덱스 계산 (동일)
    const float FrameTime = Time * FrameRate.AsDecimal();
    const int32 Frame0 = FMath::Clamp((int32)FrameTime, 0, Keys.Num() - 1);
    const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
    const float Alpha = FMath::Frac(FrameTime);

    // Slerp (구면 선형 보간)
    return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}
```

**Slerp를 사용하는 이유**:
- 선형 보간은 회전에 부적합 (비정상적인 회전 경로, 속도 불균일)
- Slerp는 구 표면의 최단 경로로 보간하여 자연스러운 회전 생성

#### 사용 예시

```cpp
// 애니메이션 생성
UAnimSequence* WalkAnim = NewObject<UAnimSequence>();
WalkAnim->FrameRate = {30, 1};  // 30fps
WalkAnim->NumberOfFrames = 90;  // 3초
WalkAnim->SequenceLength = 3.0f;

// 본 트랙 추가
FBoneAnimationTrack RootTrack(FName("Root"), 0);
for (int i = 0; i < 90; ++i)
{
    float Time = i / 30.0f;
    RootTrack.InternalTrack.PosKeys.Add(FVector(Time * 100.0f, 0, 0));
    RootTrack.InternalTrack.RotKeys.Add(FQuat::Identity);
    RootTrack.InternalTrack.ScaleKeys.Add(FVector(1, 1, 1));
}
WalkAnim->AddBoneTrack(RootTrack);

// 특정 시간의 포즈 추출
FPoseContext Pose;
FAnimExtractContext Context(1.5f, false);  // 1.5초 시점
WalkAnim->GetAnimationPose(Pose, Context);
```

---

## 런타임 클래스

### UAnimInstance

**파일**: `Source/Runtime/Engine/Animation/AnimInstance.h`

**설명**: 애니메이션 재생 로직의 베이스 클래스입니다. State Machine, Blend Tree 등을 구현하기 위해 상속받습니다.

#### 주요 멤버

```cpp
protected:
float CurrentTime;       // 현재 재생 시간
float PreviousTime;      // 이전 프레임 시간
USkeletalMeshComponent* OwnerComponent;  // 소유 컴포넌트
```

#### 주요 메서드

```cpp
// 애니메이션 업데이트 (매 프레임 호출, 오버라이드 가능)
virtual void NativeUpdateAnimation(float DeltaSeconds);

// Notify 트리거링
void TriggerAnimNotifies(float DeltaSeconds);

// 현재 시간 접근
float GetCurrentTime() const;
void SetCurrentTime(float InTime);

// 소유 컴포넌트 접근
USkeletalMeshComponent* GetOwnerComponent() const;
```

#### 사용 예시

```cpp
// 커스텀 애니메이션 인스턴스 (State Machine 구현 예시)
class UMyAnimInstance : public UAnimInstance
{
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override
    {
        Super::NativeUpdateAnimation(DeltaSeconds);

        // 커스텀 로직: State Machine 업데이트
        UpdateStateMachine(DeltaSeconds);

        // 블렌딩 로직
        BlendAnimations(DeltaSeconds);
    }

private:
    void UpdateStateMachine(float DeltaSeconds);
    void BlendAnimations(float DeltaSeconds);
};
```

---

### UAnimSingleNodeInstance

**파일**: `Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h`

**설명**: 단일 애니메이션을 재생하는 구체 클래스입니다.

#### 주요 멤버

```cpp
private:
UAnimSequence* CurrentSequence;  // 재생 중인 애니메이션
bool bIsPlaying;                 // 재생 중 여부
bool bLooping;                   // 루핑 여부
float PlayRate;                  // 재생 속도 (1.0 = 정상)
```

#### 주요 메서드

```cpp
// 애니메이션 설정
void SetAnimationAsset(UAnimSequence* NewAsset);

// 재생 제어
void Play(bool bInLooping);
void Stop();
void Pause();
void SetPlayRate(float InPlayRate);

// 재생 상태 확인
bool IsPlaying() const;
bool IsLooping() const;
float GetPlayRate() const;

// 업데이트 구현
virtual void NativeUpdateAnimation(float DeltaSeconds) override;
```

#### 재생 로직

```cpp
void NativeUpdateAnimation(float DeltaSeconds)
{
    if (!bIsPlaying || !CurrentSequence) return;

    // 1. 시간 업데이트
    PreviousTime = CurrentTime;
    CurrentTime += DeltaSeconds * PlayRate;

    // 2. 애니메이션 길이 체크
    const float AnimLength = CurrentSequence->GetPlayLength();

    // 3. 루핑 처리
    if (CurrentTime >= AnimLength)
    {
        if (bLooping)
        {
            CurrentTime = fmod(CurrentTime, AnimLength);  // 처음으로
        }
        else
        {
            CurrentTime = AnimLength;
            bIsPlaying = false;  // 정지
        }
    }

    // 4. Notify 트리거
    TriggerAnimNotifies(DeltaSeconds);
}
```

#### 사용 예시

```cpp
// 단일 애니메이션 재생
UAnimSingleNodeInstance* Instance = NewObject<UAnimSingleNodeInstance>();
Instance->SetAnimationAsset(WalkAnimation);
Instance->Play(true);  // 루핑 활성화
Instance->SetPlayRate(1.5f);  // 1.5배속

// 정지
Instance->Stop();

// 일시 정지
Instance->Pause();
```

---

## 데이터 구조

### FFrameRate

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 애니메이션의 프레임 레이트를 표현하는 구조체입니다.

#### 멤버

```cpp
int32 Numerator = 30;     // 분자 (30fps의 경우 30)
int32 Denominator = 1;    // 분모 (30fps의 경우 1)
```

#### 메서드

```cpp
// 프레임 레이트를 실수로 변환
float AsDecimal() const
{
    return static_cast<float>(Numerator) / static_cast<float>(Denominator);
}

// 시간(초) → 프레임 번호 변환
int32 AsFrameNumber(float TimeInSeconds) const
{
    return static_cast<int32>(TimeInSeconds * AsDecimal());
}

// 프레임 번호 → 시간(초) 변환
float AsSeconds(int32 FrameNumber) const
{
    return static_cast<float>(FrameNumber) / AsDecimal();
}
```

#### 사용 예시

```cpp
FFrameRate Rate = {30, 1};  // 30fps
float Fps = Rate.AsDecimal();  // 30.0
int32 Frame = Rate.AsFrameNumber(1.5f);  // 45
float Time = Rate.AsSeconds(45);  // 1.5
```

---

### FRawAnimSequenceTrack

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 본별 키프레임 데이터를 저장하는 구조체입니다.

#### 멤버

```cpp
TArray<FVector> PosKeys;      // 위치 키프레임 배열
TArray<FQuat> RotKeys;        // 회전 키프레임 배열 (Quaternion)
TArray<FVector> ScaleKeys;    // 스케일 키프레임 배열
```

#### 메서드

```cpp
// 비어있는지 확인
bool IsEmpty() const
{
    return PosKeys.IsEmpty() && RotKeys.IsEmpty() && ScaleKeys.IsEmpty();
}

// 키 개수 (가장 많은 키를 가진 트랙 기준)
int32 GetNumKeys() const
{
    int32 MaxKeys = 0;
    if (!PosKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, PosKeys.Num());
    if (!RotKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, RotKeys.Num());
    if (!ScaleKeys.IsEmpty()) MaxKeys = FMath::Max(MaxKeys, ScaleKeys.Num());
    return MaxKeys;
}
```

#### 사용 예시

```cpp
FRawAnimSequenceTrack Track;

// 위치 키프레임 추가
Track.PosKeys.Add(FVector(0, 0, 0));    // 프레임 0
Track.PosKeys.Add(FVector(100, 0, 0));  // 프레임 1
Track.PosKeys.Add(FVector(200, 0, 0));  // 프레임 2

// 회전 키프레임 추가
Track.RotKeys.Add(FQuat::Identity);
Track.RotKeys.Add(FQuat::FromAxisAngle(FVector::UpVector, FMath::PI / 2));

// 스케일 키프레임 추가
Track.ScaleKeys.Add(FVector(1, 1, 1));
Track.ScaleKeys.Add(FVector(2, 2, 2));
```

---

### FBoneAnimationTrack

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 본과 키프레임 데이터를 연결하는 구조체입니다.

#### 멤버

```cpp
FName Name;                           // 본 이름 (예: "Spine", "Head")
int32 BoneTreeIndex = -1;             // 스켈레톤에서의 본 인덱스
FRawAnimSequenceTrack InternalTrack;  // 실제 키프레임 데이터
```

#### 생성자

```cpp
FBoneAnimationTrack() = default;

FBoneAnimationTrack(const FName& InName, int32 InBoneIndex)
    : Name(InName), BoneTreeIndex(InBoneIndex) {}
```

#### 사용 예시

```cpp
// 본 트랙 생성
FBoneAnimationTrack SpineTrack(FName("Spine"), 1);

// 키프레임 데이터 추가
SpineTrack.InternalTrack.PosKeys.Add(FVector(0, 0, 100));
SpineTrack.InternalTrack.RotKeys.Add(FQuat::Identity);
SpineTrack.InternalTrack.ScaleKeys.Add(FVector(1, 1, 1));

// 애니메이션에 추가
AnimSequence->AddBoneTrack(SpineTrack);
```

---

### FAnimNotifyEvent

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 애니메이션 이벤트를 표현하는 구조체입니다.

#### 멤버

```cpp
float TriggerTime = 0.0f;     // 트리거 시간 (초)
float Duration = 0.0f;        // 지속 시간 (0 = 순간 이벤트)
FName NotifyName;             // Notify 이름 (예: "Footstep", "Shoot")
FString NotifyData;           // 추가 데이터 (JSON 등)
```

#### 생성자

```cpp
FAnimNotifyEvent() = default;

FAnimNotifyEvent(float InTime, const FName& InName)
    : TriggerTime(InTime), NotifyName(InName) {}
```

#### 사용 예시

```cpp
// 발소리 이벤트 추가 (0.3초 시점)
FAnimNotifyEvent Footstep(0.3f, FName("Footstep"));
Footstep.NotifyData = "{\"volume\": 0.8, \"pitch\": 1.0}";
WalkAnimation->Notifies.Add(Footstep);

// 무기 발사 이벤트 (0.5초 시점, 0.1초 지속)
FAnimNotifyEvent FireWeapon(0.5f, FName("FireWeapon"));
FireWeapon.Duration = 0.1f;
FireWeapon.NotifyData = "{\"projectile\": \"Bullet\"}";
AttackAnimation->Notifies.Add(FireWeapon);

// Notify 핸들링
void HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    if (Notify.NotifyName == FName("Footstep"))
    {
        PlaySound("Footstep.wav");
    }
    else if (Notify.NotifyName == FName("FireWeapon"))
    {
        SpawnProjectile(Notify.NotifyData);
    }
}
```

---

### FAnimExtractContext

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 포즈 추출 시 필요한 컨텍스트 정보를 담는 구조체입니다.

#### 멤버

```cpp
float CurrentTime = 0.0f;          // 현재 시간 (초)
bool bExtractRootMotion = false;   // 루트 모션 추출 여부
bool bLooping = false;             // 루핑 여부
```

#### 생성자

```cpp
FAnimExtractContext() = default;

FAnimExtractContext(float InTime, bool InLooping)
    : CurrentTime(InTime), bLooping(InLooping) {}
```

#### 사용 예시

```cpp
// 1.5초 시점의 포즈 추출 (비루핑)
FAnimExtractContext Context(1.5f, false);
FPoseContext Pose;
AnimSequence->GetAnimationPose(Pose, Context);

// 루핑 애니메이션에서 포즈 추출
FAnimExtractContext LoopContext(2.5f, true);
AnimSequence->GetAnimationPose(Pose, LoopContext);
```

---

### FPoseContext

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 전체 스켈레톤의 포즈 스냅샷을 저장하는 구조체입니다.

#### 멤버

```cpp
TArray<FTransform> BoneTransforms;  // 모든 본의 로컬 트랜스폼
```

#### 메서드

```cpp
// 본 개수 설정 및 초기화
void SetNumBones(int32 NumBones)
{
    BoneTransforms.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        BoneTransforms[i] = FTransform();  // Identity로 초기화
    }
}

// 본 개수 반환
int32 GetNumBones() const { return BoneTransforms.Num(); }
```

#### 사용 예시

```cpp
// 포즈 생성 및 초기화
FPoseContext Pose;
Pose.SetNumBones(50);  // 50개 본

// 특정 본의 트랜스폼 설정
Pose.BoneTransforms[0] = FTransform(FVector(0, 0, 0), FQuat::Identity, FVector(1, 1, 1));
Pose.BoneTransforms[1] = FTransform(FVector(0, 0, 50), FQuat::Identity, FVector(1, 1, 1));

// 포즈 블렌딩
FPoseContext WalkPose, RunPose, BlendedPose;
FAnimationRuntime::BlendTwoPosesTogether(WalkPose, RunPose, 0.5f, BlendedPose);
```

---

### EAnimationMode

**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**설명**: 애니메이션 재생 모드를 정의하는 열거형입니다.

#### 값

```cpp
enum class EAnimationMode : uint8
{
    AnimationSingleNode,   // 단일 애니메이션 재생
    AnimationLuaScript,    // Lua 스크립트 기반 애니메이션
};
```

#### 사용 예시

```cpp
// 단일 노드 모드 설정
SkelMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);

// Lua 스크립트 모드 설정
SkelMeshComponent->SetAnimationMode(EAnimationMode::AnimationLuaScript);
```

---

## 컴포넌트 통합

### USkeletalMeshComponent

**파일**: `Source/Runtime/Engine/Components/SkeletalMeshComponent.h`

**설명**: 스켈레탈 메쉬 렌더링 및 애니메이션 재생을 담당하는 컴포넌트입니다.

#### 애니메이션 관련 멤버

```cpp
// 애니메이션 모드
UPROPERTY(EditAnywhere, Category="[애니메이션]")
EAnimationMode AnimationMode;

// 애니메이션 인스턴스
UPROPERTY(EditAnywhere, Category="[애니메이션]")
UAnimInstance* AnimInstance;

// 단일 노드 모드용 애니메이션
UPROPERTY(EditAnywhere, Category="[애니메이션]")
UAnimSequence* AnimationData;
```

#### 주요 메서드

```cpp
// 애니메이션 재생 (Lua 바인딩)
UFUNCTION(DisplayName="애니메이션_재생", LuaBind)
void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping);

// 애니메이션 정지 (Lua 바인딩)
UFUNCTION(DisplayName="애니메이션_정지", LuaBind)
void StopAnimation();

// 애니메이션 모드 설정
void SetAnimationMode(EAnimationMode InMode);

// 애니메이션 설정
void SetAnimation(UAnimSequence* InAnim);

// 재생 시작
void Play(bool bLooping);

// AnimNotify 핸들링
void HandleAnimNotify(const FAnimNotifyEvent& Notify);

// 애니메이션 틱 (protected)
void TickAnimation(float DeltaTime);
```

#### 애니메이션 재생 흐름

```cpp
// 1. PlayAnimation 호출
void USkeletalMeshComponent::PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping)
{
    // 단일 노드 모드로 설정
    SetAnimationMode(EAnimationMode::AnimationSingleNode);

    // 애니메이션 설정
    SetAnimation(NewAnimToPlay);

    // 재생 시작
    Play(bLooping);
}

// 2. TickComponent에서 호출
void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // 애니메이션 업데이트
    TickAnimation(DeltaTime);
}

// 3. TickAnimation 구현
void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (!AnimInstance) return;

    // 애니메이션 인스턴스 업데이트
    AnimInstance->NativeUpdateAnimation(DeltaTime);

    // 포즈 추출
    FPoseContext Pose;
    FAnimExtractContext Context(AnimInstance->GetCurrentTime(), false);
    AnimationData->GetAnimationPose(Pose, Context);

    // 포즈를 본 트랜스폼에 적용
    // TODO: BoneSpaceTransforms 업데이트
    // TODO: CPU Skinning 수행
}
```

#### 사용 예시

##### C++에서 사용

```cpp
// 컴포넌트 가져오기
USkeletalMeshComponent* SkelMesh = Actor->GetComponent<USkeletalMeshComponent>();

// 애니메이션 재생
UAnimSequence* WalkAnim = LoadAnimation("Character/Walk.anim");
SkelMesh->PlayAnimation(WalkAnim, true);  // 루핑

// 정지
SkelMesh->StopAnimation();
```

##### Lua에서 사용

```lua
-- 스켈레탈 메쉬 컴포넌트 가져오기
local skelMesh = actor:GetComponent("USkeletalMeshComponent")

-- 애니메이션 재생 (루핑)
skelMesh:애니메이션_재생(walkAnim, true)

-- 애니메이션 정지
skelMesh:애니메이션_정지()
```

---

## 유틸리티 클래스

### FAnimationRuntime

**파일**: `Source/Runtime/Engine/Animation/AnimationRuntime.h`

**설명**: 애니메이션 블렌딩 및 유틸리티 함수를 제공하는 정적 클래스입니다.

#### 주요 메서드

##### BlendTwoPosesTogether

```cpp
static void BlendTwoPosesTogether(
    const FPoseContext& PoseA,      // 첫 번째 포즈
    const FPoseContext& PoseB,      // 두 번째 포즈
    float BlendAlpha,               // 블렌딩 비율 (0.0 ~ 1.0)
    FPoseContext& OutPose           // 결과 포즈
);
```

**설명**: 두 포즈를 지정된 비율로 블렌딩합니다.

**매개변수**:
- `PoseA`: 첫 번째 포즈 (Alpha = 0.0일 때 100%)
- `PoseB`: 두 번째 포즈 (Alpha = 1.0일 때 100%)
- `BlendAlpha`: 블렌딩 비율 (0.0 = A, 0.5 = 50/50, 1.0 = B)
- `OutPose`: 블렌딩된 결과 포즈

**사용 예시**:

```cpp
// 걷기 → 달리기 전환 (0.5초에 걸쳐)
FPoseContext WalkPose, RunPose, BlendedPose;

// 각 포즈 추출
WalkAnim->GetAnimationPose(WalkPose, FAnimExtractContext(Time, false));
RunAnim->GetAnimationPose(RunPose, FAnimExtractContext(Time, false));

// 블렌딩 비율 계산 (0.0 → 1.0)
float TransitionProgress = TransitionTime / 0.5f;
float BlendAlpha = FMath::Clamp(TransitionProgress, 0.0f, 1.0f);

// 블렌딩 수행
FAnimationRuntime::BlendTwoPosesTogether(WalkPose, RunPose, BlendAlpha, BlendedPose);

// 결과 포즈 적용
ApplyPoseToSkeleton(BlendedPose);
```

##### BlendTransforms

```cpp
static FTransform BlendTransforms(
    const FTransform& A,     // 첫 번째 트랜스폼
    const FTransform& B,     // 두 번째 트랜스폼
    float Alpha              // 블렌딩 비율 (0.0 ~ 1.0)
);
```

**설명**: 두 트랜스폼을 지정된 비율로 블렌딩합니다.

**알고리즘**:
- **Position**: 선형 보간 (Lerp)
- **Rotation**: 구면 선형 보간 (Slerp)
- **Scale**: 선형 보간 (Lerp)

**사용 예시**:

```cpp
// 두 본 트랜스폼 블렌딩
FTransform TransformA(FVector(0, 0, 0), FQuat::Identity, FVector(1, 1, 1));
FTransform TransformB(FVector(100, 0, 0), FQuat::FromAxisAngle(FVector::UpVector, FMath::PI), FVector(2, 2, 2));

// 50% 블렌딩
FTransform Blended = FAnimationRuntime::BlendTransforms(TransformA, TransformB, 0.5f);

// 결과:
// Position: (50, 0, 0)
// Rotation: 90도 회전
// Scale: (1.5, 1.5, 1.5)
```

---

## 사용 가이드

### 시나리오 1: 단일 애니메이션 재생

```cpp
// 1. 애니메이션 로드 (또는 생성)
UAnimSequence* WalkAnim = LoadAnimation("Character/Walk.anim");

// 2. 스켈레탈 메쉬 컴포넌트 가져오기
USkeletalMeshComponent* SkelMesh = Character->GetComponent<USkeletalMeshComponent>();

// 3. 애니메이션 재생
SkelMesh->PlayAnimation(WalkAnim, true);  // 루핑 활성화
```

### 시나리오 2: 애니메이션 블렌딩

```cpp
// 1. 두 포즈 추출
FPoseContext IdlePose, WalkPose, BlendedPose;

FAnimExtractContext Context(CurrentTime, false);
IdleAnim->GetAnimationPose(IdlePose, Context);
WalkAnim->GetAnimationPose(WalkPose, Context);

// 2. 속도에 따라 블렌딩 비율 계산
float Speed = Character->GetVelocity().Size();
float MaxWalkSpeed = 400.0f;
float BlendAlpha = FMath::Clamp(Speed / MaxWalkSpeed, 0.0f, 1.0f);

// 3. 블렌딩 수행
FAnimationRuntime::BlendTwoPosesTogether(IdlePose, WalkPose, BlendAlpha, BlendedPose);

// 4. 블렌딩된 포즈 적용
ApplyPoseToSkeleton(BlendedPose);
```

### 시나리오 3: AnimNotify 활용

```cpp
// 1. 애니메이션에 Notify 추가
FAnimNotifyEvent FootstepLeft(0.3f, FName("FootstepLeft"));
FAnimNotifyEvent FootstepRight(0.8f, FName("FootstepRight"));

WalkAnim->Notifies.Add(FootstepLeft);
WalkAnim->Notifies.Add(FootstepRight);

// 2. Notify 핸들링
void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    if (Notify.NotifyName == FName("FootstepLeft"))
    {
        SpawnFootstepEffect(LeftFootLocation);
        PlaySound("Footstep_Left.wav");
    }
    else if (Notify.NotifyName == FName("FootstepRight"))
    {
        SpawnFootstepEffect(RightFootLocation);
        PlaySound("Footstep_Right.wav");
    }
}
```

### 시나리오 4: 커스텀 AnimInstance (State Machine)

```cpp
// 1. UAnimInstance 상속
class UCharacterAnimInstance : public UAnimInstance
{
public:
    enum class EAnimState
    {
        Idle,
        Walk,
        Run,
        Jump
    };

    virtual void NativeUpdateAnimation(float DeltaSeconds) override
    {
        Super::NativeUpdateAnimation(DeltaSeconds);

        // State Machine 업데이트
        UpdateState();

        // 포즈 블렌딩
        BlendCurrentState(DeltaSeconds);
    }

private:
    EAnimState CurrentState = EAnimState::Idle;
    EAnimState PreviousState = EAnimState::Idle;
    float StateTransitionTime = 0.0f;

    void UpdateState()
    {
        // 상태 전환 로직
        float Speed = GetOwnerComponent()->GetOwner()->GetVelocity().Size();

        if (Speed < 10.0f)
            TransitionToState(EAnimState::Idle);
        else if (Speed < 300.0f)
            TransitionToState(EAnimState::Walk);
        else
            TransitionToState(EAnimState::Run);
    }

    void TransitionToState(EAnimState NewState)
    {
        if (CurrentState != NewState)
        {
            PreviousState = CurrentState;
            CurrentState = NewState;
            StateTransitionTime = 0.0f;
        }
    }

    void BlendCurrentState(float DeltaSeconds)
    {
        // 전환 중이면 블렌딩
        const float TransitionDuration = 0.3f;
        StateTransitionTime += DeltaSeconds;

        if (StateTransitionTime < TransitionDuration)
        {
            float Alpha = StateTransitionTime / TransitionDuration;

            FPoseContext PrevPose, CurrPose, BlendedPose;
            GetPoseForState(PreviousState, PrevPose);
            GetPoseForState(CurrentState, CurrPose);

            FAnimationRuntime::BlendTwoPosesTogether(PrevPose, CurrPose, Alpha, BlendedPose);
            ApplyPose(BlendedPose);
        }
        else
        {
            // 전환 완료
            FPoseContext Pose;
            GetPoseForState(CurrentState, Pose);
            ApplyPose(Pose);
        }
    }
};
```

### 시나리오 5: 더미 애니메이션 데이터 생성

```cpp
// 테스트용 회전 애니메이션 생성
UAnimSequence* CreateRotationAnimation()
{
    UAnimSequence* Anim = NewObject<UAnimSequence>();

    // 메타데이터 설정
    Anim->FrameRate = {30, 1};      // 30fps
    Anim->NumberOfFrames = 90;      // 3초
    Anim->NumberOfKeys = 90;
    Anim->SequenceLength = 3.0f;

    // 루트 본 트랙 생성
    FBoneAnimationTrack RootTrack(FName("Root"), 0);

    // 키프레임 추가 (360도 회전)
    for (int i = 0; i < 90; ++i)
    {
        float Angle = (i / 90.0f) * 360.0f;
        float Radians = FMath::DegreesToRadians(Angle);

        // 위치: 고정
        RootTrack.InternalTrack.PosKeys.Add(FVector(0, 0, 0));

        // 회전: Y축 중심으로 회전
        FQuat Rotation = FQuat::FromAxisAngle(FVector::UpVector, Radians);
        RootTrack.InternalTrack.RotKeys.Add(Rotation);

        // 스케일: 고정
        RootTrack.InternalTrack.ScaleKeys.Add(FVector(1, 1, 1));
    }

    Anim->AddBoneTrack(RootTrack);

    return Anim;
}
```

---

## API 레퍼런스

### 클래스 요약

| 클래스 | 타입 | 설명 |
|--------|------|------|
| `UAnimationAsset` | 추상 | 애니메이션 에셋 베이스 클래스 |
| `UAnimSequenceBase` | 추상 | 재생 가능한 애니메이션 베이스 |
| `UAnimSequence` | 구체 | 키프레임 애니메이션 |
| `UAnimInstance` | 추상 | 애니메이션 재생 로직 베이스 |
| `UAnimSingleNodeInstance` | 구체 | 단일 애니메이션 재생기 |
| `USkeletalMeshComponent` | 구체 | 애니메이션 재생 컴포넌트 |
| `FAnimationRuntime` | 정적 | 블렌딩 유틸리티 |

### 구조체 요약

| 구조체 | 설명 |
|--------|------|
| `FFrameRate` | 프레임 레이트 표현 |
| `FRawAnimSequenceTrack` | 본별 키프레임 데이터 |
| `FBoneAnimationTrack` | 본과 키프레임 연결 |
| `FAnimNotifyEvent` | 애니메이션 이벤트 |
| `FAnimExtractContext` | 포즈 추출 컨텍스트 |
| `FPoseContext` | 전체 스켈레톤 포즈 |

### 열거형 요약

| 열거형 | 값 | 설명 |
|--------|-----|------|
| `EAnimationMode` | `AnimationSingleNode` | 단일 애니메이션 재생 |
| | `AnimationLuaScript` | Lua 스크립트 기반 |

### 주요 메서드 요약

#### UAnimSequence

| 메서드 | 반환 타입 | 설명 |
|--------|-----------|------|
| `GetAnimationPose()` | `void` | 특정 시간의 포즈 추출 |
| `GetBoneTransformAtTime()` | `FTransform` | 특정 본의 트랜스폼 계산 |
| `AddBoneTrack()` | `void` | 본 트랙 추가 |
| `GetBoneAnimationTracks()` | `const TArray&` | 모든 본 트랙 반환 |

#### UAnimInstance

| 메서드 | 반환 타입 | 설명 |
|--------|-----------|------|
| `NativeUpdateAnimation()` | `void` | 매 프레임 업데이트 (가상) |
| `TriggerAnimNotifies()` | `void` | Notify 트리거 |
| `GetCurrentTime()` | `float` | 현재 재생 시간 |

#### UAnimSingleNodeInstance

| 메서드 | 반환 타입 | 설명 |
|--------|-----------|------|
| `SetAnimationAsset()` | `void` | 애니메이션 설정 |
| `Play()` | `void` | 재생 시작 |
| `Stop()` | `void` | 재생 정지 |
| `Pause()` | `void` | 일시 정지 |
| `SetPlayRate()` | `void` | 재생 속도 설정 |

#### USkeletalMeshComponent

| 메서드 | 반환 타입 | 설명 | Lua |
|--------|-----------|------|-----|
| `PlayAnimation()` | `void` | 애니메이션 재생 | ✓ |
| `StopAnimation()` | `void` | 애니메이션 정지 | ✓ |
| `SetAnimationMode()` | `void` | 애니메이션 모드 설정 | |
| `HandleAnimNotify()` | `void` | Notify 핸들링 | |

#### FAnimationRuntime

| 메서드 | 반환 타입 | 설명 |
|--------|-----------|------|
| `BlendTwoPosesTogether()` | `void` | 두 포즈 블렌딩 |
| `BlendTransforms()` | `FTransform` | 두 트랜스폼 블렌딩 |

---

## 부록

### 미구현 기능

다음 기능들은 현재 TODO 상태입니다:

1. **FBX 애니메이션 임포트**
   - `UFbxLoader::LoadAnimationFromFbx()` 구현 필요
   - FbxAnimStack, FbxAnimLayer, FbxAnimCurve에서 키프레임 추출

2. **직렬화 (Serialize)**
   - 모든 애니메이션 클래스의 Serialize() 구현 필요
   - JSON 형식으로 저장/로드

3. **AnimNotify 트리거 로직**
   - `UAnimInstance::TriggerAnimNotifies()` 완전 구현
   - `AActor::HandleAnimNotify()` 가상 함수 추가

4. **루트 모션 (Root Motion)**
   - `FAnimExtractContext::bExtractRootMotion` 처리
   - 루트 본의 이동을 캐릭터 이동에 적용

### 확장 가능한 기능

애니메이션 시스템은 다음 기능들로 확장 가능합니다:

1. **Animation State Machine**
   - `UAnimInstance`를 상속받아 구현
   - 상태 전환 조건 및 블렌딩 로직

2. **Blend Space**
   - 2D 파라미터 공간에서 애니메이션 블렌딩
   - 예: 이동 방향과 속도에 따른 블렌딩

3. **Animation Montage**
   - 섹션 기반 애니메이션 재생
   - 중간 섹션 점프, 루핑 등

4. **IK (Inverse Kinematics)**
   - Two-Bone IK, Look-At 등
   - 런타임 본 트랜스폼 수정

5. **Animation Layers**
   - 상/하체 애니메이션 독립 재생
   - 마스크 기반 레이어 블렌딩

### 성능 고려사항

1. **보간 최적화**
   - 현재: 매 프레임 모든 본 보간
   - 개선: LOD 기반 본 선택적 업데이트

2. **캐싱**
   - 자주 사용되는 포즈 캐싱
   - 프레임 인덱스 계산 결과 재사용

3. **멀티스레딩**
   - 애니메이션 업데이트를 워커 스레드로 이동
   - 포즈 블렌딩 병렬화

### 참고 자료

- 언리얼 엔진 애니메이션 시스템: https://docs.unrealengine.com/en-US/AnimatingObjects/
- 구면 선형 보간 (Slerp): https://en.wikipedia.org/wiki/Slerp
- 키프레임 애니메이션: https://en.wikipedia.org/wiki/Key_frame

---

**문서 작성자**: Claude Code
**최종 업데이트**: 2025-11-14
**피드백**: Mundi Engine 개발팀
