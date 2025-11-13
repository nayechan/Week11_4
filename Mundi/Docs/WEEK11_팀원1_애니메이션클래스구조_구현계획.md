# Mundi Engine 애니메이션 클래스 계층 구조 구현 계획

**작성일**: 2025-11-13
**담당**: 팀원1 (애니메이션 코어 & FBX 임포트)
**목표**: 팀원2가 Day 3까지 작업을 시작할 수 있도록 애니메이션 클래스 기본 구조 구축

---

## 🎯 목표

팀원2(AnimInstance & State Machine 담당)가 **Day 3까지 작업을 시작할 수 있도록** 애니메이션 클래스 기본 구조와 인터페이스를 구축합니다.

### 완료 조건
- 애니메이션 클래스 계층 구조 완성 (UAnimationAsset → UAnimSequenceBase → UAnimSequence)
- 팀원2가 사용할 인터페이스 준비 완료
- 빌드 성공 및 기본 동작 확인

---

## 📊 현재 Mundi Engine 인프라 분석

### ✅ 이미 구현되어 있는 것들

#### 1. Skeletal System (완전 구현됨)
**위치**: `Source/Runtime/Core/Misc/VertexData.h`

- `FSkeleton` - 스켈레톤 데이터 (본 배열, 이름 검색 맵)
- `FBone` - 개별 본 (이름, 부모 인덱스, Bind Pose, Inverse Bind Pose)
- `FSkeletalMeshData` - 스켈레탈 메쉬 데이터
- `FSkinnedVertex` - 본 가중치/인덱스 포함 버텍스 (최대 4개 본)

#### 2. Component System (완전 구현됨)
**위치**: `Source/Runtime/Engine/Components/`

- `USkinnedMeshComponent` - CPU 스키닝 수행
- `USkeletalMeshComponent` - 본 트랜스폼 관리 및 스키닝 매트릭스 계산

#### 3. Asset System (완전 구현됨)
**위치**: `Source/Runtime/AssetManagement/`

- `USkeletalMesh` - 스켈레탈 메쉬 에셋
- `UResourceBase` - 모든 에셋의 베이스 클래스
- `UResourceManager` - 싱글톤 에셋 로더/캐시

#### 4. FBX Loader (스켈레톤 로딩 완료)
**위치**: `Source/Editor/FBXLoader.h`

- `UFbxLoader` - FBX SDK 2020.3.7 사용
- 스켈레톤 및 스켈레탈 메쉬 로딩 완료
- FBX 애니메이션 API 접근 가능 (FbxAnimStack, FbxAnimLayer, FbxAnimCurve)

#### 5. Math System (완전 구현됨)
**위치**: `Source/Runtime/Core/Math/Vector.h`

- `FVector` - 3D 벡터
- `FQuat` - Quaternion (X, Y, Z, W)
- `FTransform` - 완전한 트랜스폼 (Translation, Rotation, Scale3D)
- `FMatrix` - 4x4 행렬 (SIMD 최적화)
- `FMath::Lerp()`, `FQuat::Slerp()` - 보간 유틸리티

#### 6. Reflection System (자동 코드 생성)
**위치**: `Tools/CodeGenerator/`

- `UCLASS`, `UPROPERTY`, `UFUNCTION` 매크로 지원
- Python 기반 자동 코드 생성 (`generate.py`)
- `.generated.h/.cpp` 파일 자동 생성

### ❌ 구현되지 않은 것들 (이번 작업 범위)

#### Animation 디렉토리 및 클래스
- `Source/Runtime/Engine/Animation/` 디렉토리 (생성 필요)
- 모든 애니메이션 클래스 (UAnimationAsset, UAnimSequence 등)
- 애니메이션 데이터 구조 (FRawAnimSequenceTrack, FBoneAnimationTrack)
- 애니메이션 인스턴스 시스템 (UAnimInstance)
- FBX 애니메이션 임포트 로직

---

## 📋 Phase 1: 디렉토리 생성 및 데이터 구조 정의

### 1.1 디렉토리 생성
```
Source/Runtime/Engine/Animation/
```

### 1.2 AnimationTypes.h 생성
**파일**: `Source/Runtime/Engine/Animation/AnimationTypes.h`

**목적**: 모든 애니메이션 관련 데이터 구조 정의

**포함 내용**:

```cpp
#pragma once
#include "Core/Math/Vector.h"
#include "Core/Containers/UEContainer.h"

// 프레임 레이트 구조체
struct FFrameRate
{
    int32 Numerator = 30;
    int32 Denominator = 1;

    float AsDecimal() const
    {
        return static_cast<float>(Numerator) / static_cast<float>(Denominator);
    }

    // 시간 → 프레임 변환
    int32 AsFrameNumber(float TimeInSeconds) const
    {
        return static_cast<int32>(TimeInSeconds * AsDecimal());
    }

    // 프레임 → 시간 변환
    float AsSeconds(int32 FrameNumber) const
    {
        return static_cast<float>(FrameNumber) / AsDecimal();
    }
};

// Raw 애니메이션 키프레임 (발제 문서 기준)
struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;      // 위치 키프레임
    TArray<FQuat> RotKeys;        // 회전 키프레임 (Quaternion)
    TArray<FVector> ScaleKeys;    // 스케일 키프레임

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
};

// 본별 애니메이션 트랙 (발제 문서 기준)
struct FBoneAnimationTrack
{
    FName Name;                           // Bone 이름
    int32 BoneTreeIndex = -1;             // 스켈레톤 본 인덱스
    FRawAnimSequenceTrack InternalTrack;  // 실제 애니메이션 데이터

    FBoneAnimationTrack() = default;
    FBoneAnimationTrack(const FName& InName, int32 InBoneIndex)
        : Name(InName), BoneTreeIndex(InBoneIndex) {}
};

// AnimNotify 이벤트 (발제 문서 요구사항)
struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;     // 트리거 시간 (초)
    float Duration = 0.0f;         // 지속 시간 (0 = 순간 이벤트)
    FName NotifyName;              // Notify 이름 (예: "Footstep", "Shoot")
    FString NotifyData;            // 추가 데이터 (JSON 등)

    FAnimNotifyEvent() = default;
    FAnimNotifyEvent(float InTime, const FName& InName)
        : TriggerTime(InTime), NotifyName(InName) {}
};

// 애니메이션 추출 컨텍스트 (팀원2가 사용)
struct FAnimExtractContext
{
    float CurrentTime = 0.0f;          // 현재 시간 (초)
    bool bExtractRootMotion = false;   // 루트 모션 추출 여부
    bool bLooping = false;              // 루핑 여부

    FAnimExtractContext() = default;
    FAnimExtractContext(float InTime, bool InLooping)
        : CurrentTime(InTime), bLooping(InLooping) {}
};

// 포즈 데이터 컨테이너 (팀원2가 블렌딩에 사용)
struct FPoseContext
{
    TArray<FTransform> BoneTransforms;  // 모든 본의 로컬 트랜스폼

    FPoseContext() = default;

    void SetNumBones(int32 NumBones)
    {
        BoneTransforms.SetNum(NumBones);
        // Identity transform으로 초기화
        for (int32 i = 0; i < NumBones; ++i)
        {
            BoneTransforms[i] = FTransform::Identity;
        }
    }

    int32 GetNumBones() const { return BoneTransforms.Num(); }
};

// 애니메이션 모드 열거형
enum class EAnimationMode : uint8
{
    AnimationSingleNode,   // 단일 애니메이션 재생
    AnimationBlueprint,    // 애니메이션 블루프린트 (미래 확장)
};
```

---

## 📋 Phase 2: 애니메이션 에셋 클래스 계층 구조

### 2.1 UAnimationAsset (베이스 클래스)

**파일**: `Source/Runtime/Engine/Animation/AnimationAsset.h`

```cpp
#pragma once
#include "AssetManagement/ResourceBase.h"
#include "AnimationTypes.h"
#include "UAnimationAsset.generated.h"

UCLASS(DisplayName="애니메이션 에셋", Description="애니메이션 에셋 베이스 클래스")
class UAnimationAsset : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimationAsset() = default;
    virtual ~UAnimationAsset() = default;

    // 애니메이션 길이 반환 (순수 가상)
    virtual float GetPlayLength() const { return 0.0f; }

    // 스켈레톤 참조
    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="대상 스켈레톤")
    class USkeleton* Skeleton = nullptr;

    // 메타데이터
    UPROPERTY(EditAnywhere, Category="[애니메이션]")
    TArray<class UAnimMetaData*> MetaData;

    // 직렬화
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimationAsset.cpp`

```cpp
#include "pch.h"
#include "AnimationAsset.h"
#include "GlobalConsole.h"

void UAnimationAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // 현재는 비어있음 (스켈레톤은 참조만 저장)
    // TODO: 스켈레톤 경로 직렬화
}
```

---

### 2.2 UAnimSequenceBase (중간 클래스)

**파일**: `Source/Runtime/Engine/Animation/AnimSequenceBase.h`

```cpp
#pragma once
#include "AnimationAsset.h"
#include "UAnimSequenceBase.generated.h"

UCLASS(DisplayName="애니메이션 시퀀스 베이스", Description="재생 가능한 애니메이션 베이스")
class UAnimSequenceBase : public UAnimationAsset
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimSequenceBase() = default;
    virtual ~UAnimSequenceBase() = default;

    // Notify 이벤트 배열 (발제 문서 요구사항)
    UPROPERTY(EditAnywhere, Category="[애니메이션|Notify]", Tooltip="애니메이션 알림 이벤트")
    TArray<FAnimNotifyEvent> Notifies;

    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="애니메이션 길이 (초)")
    float SequenceLength = 0.0f;

    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="재생 속도 배율", Range="0.1, 10.0")
    float RateScale = 1.0f;

    // 포즈 추출 (순수 가상 - 팀원2가 사용할 인터페이스)
    virtual void GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context) = 0;

    // 시간 범위 내의 Notify 가져오기
    void GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies) const;

    // 애니메이션 길이 반환
    virtual float GetPlayLength() const override { return SequenceLength; }

    // 직렬화
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimSequenceBase.cpp`

```cpp
#include "pch.h"
#include "AnimSequenceBase.h"
#include "GlobalConsole.h"

void UAnimSequenceBase::GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies) const
{
    OutNotifies.Empty();

    for (const FAnimNotifyEvent& Notify : Notifies)
    {
        // 시작 시간과 끝 시간 사이에 있는 Notify만 추가
        if (Notify.TriggerTime >= StartTime && Notify.TriggerTime <= EndTime)
        {
            OutNotifies.Add(Notify);
        }
    }
}

void UAnimSequenceBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // TODO: Notifies 직렬화
    // TODO: SequenceLength, RateScale 직렬화
}
```

---

### 2.3 UAnimSequence (구체 클래스)

**파일**: `Source/Runtime/Engine/Animation/AnimSequence.h`

```cpp
#pragma once
#include "AnimSequenceBase.h"
#include "UAnimSequence.generated.h"

UCLASS(DisplayName="애니메이션 시퀀스", Description="키프레임 애니메이션 데이터")
class UAnimSequence : public UAnimSequenceBase
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimSequence() = default;
    virtual ~UAnimSequence() = default;

    // 프레임 레이트
    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="프레임 레이트")
    FFrameRate FrameRate;

    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="총 프레임 수")
    int32 NumberOfFrames = 0;

    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="총 키 개수")
    int32 NumberOfKeys = 0;

    // 포즈 추출 구현
    virtual void GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context) override;

    // 특정 시간의 본 트랜스폼 가져오기 (보간)
    FTransform GetBoneTransformAtTime(int32 BoneIndex, float Time) const;

    // 본 애니메이션 트랙 접근자 (팀원2가 사용)
    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const { return BoneAnimationTracks; }

    // 본 트랙 추가 (FBX Loader가 사용)
    void AddBoneTrack(const FBoneAnimationTrack& Track) { BoneAnimationTracks.Add(Track); }
    void SetBoneTracks(const TArray<FBoneAnimationTrack>& Tracks) { BoneAnimationTracks = Tracks; }

    // 직렬화
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
    // 본별 애니메이션 트랙 (발제 문서 구조)
    TArray<FBoneAnimationTrack> BoneAnimationTracks;

    // FBX Loader가 데이터를 채울 수 있도록
    friend class UFbxLoader;

    // 보간 헬퍼 함수
    FVector InterpolatePosition(const TArray<FVector>& Keys, float Time) const;
    FQuat InterpolateRotation(const TArray<FQuat>& Keys, float Time) const;
    FVector InterpolateScale(const TArray<FVector>& Keys, float Time) const;
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimSequence.cpp`

```cpp
#include "pch.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"
#include "Core/Misc/VertexData.h" // FSkeleton

void UAnimSequence::GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context)
{
    // 스켈레톤이 없으면 실패
    if (!Skeleton)
    {
        UE_LOG("UAnimSequence::GetAnimationPose - No skeleton assigned");
        return;
    }

    // 본 개수만큼 포즈 초기화
    const int32 NumBones = Skeleton->Bones.Num();
    OutPose.SetNumBones(NumBones);

    // 현재는 빈 구현: 모든 본을 identity transform으로 설정
    // TODO: Context.CurrentTime에 맞춰 실제 애니메이션 트랙에서 보간

    // 각 본에 대해 애니메이션 적용
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        if (BoneIndex < BoneAnimationTracks.Num())
        {
            OutPose.BoneTransforms[BoneIndex] = GetBoneTransformAtTime(BoneIndex, Context.CurrentTime);
        }
        else
        {
            OutPose.BoneTransforms[BoneIndex] = FTransform::Identity;
        }
    }
}

FTransform UAnimSequence::GetBoneTransformAtTime(int32 BoneIndex, float Time) const
{
    // 인덱스 범위 체크
    if (BoneIndex < 0 || BoneIndex >= BoneAnimationTracks.Num())
    {
        return FTransform::Identity;
    }

    const FBoneAnimationTrack& Track = BoneAnimationTracks[BoneIndex];
    const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;

    // 빈 트랙이면 identity
    if (RawTrack.IsEmpty())
    {
        return FTransform::Identity;
    }

    // 각 컴포넌트 보간
    FVector Position = InterpolatePosition(RawTrack.PosKeys, Time);
    FQuat Rotation = InterpolateRotation(RawTrack.RotKeys, Time);
    FVector Scale = InterpolateScale(RawTrack.ScaleKeys, Time);

    return FTransform(Position, Rotation, Scale);
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& Keys, float Time) const
{
    if (Keys.IsEmpty())
        return FVector(0, 0, 0);

    if (Keys.Num() == 1)
        return Keys[0]; // 상수 트랙

    // 프레임 인덱스 계산
    const float FrameTime = Time * FrameRate.AsDecimal();
    const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
    const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
    const float Alpha = FMath::Frac(FrameTime);

    // 선형 보간
    return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& Keys, float Time) const
{
    if (Keys.IsEmpty())
        return FQuat::Identity;

    if (Keys.Num() == 1)
        return Keys[0]; // 상수 트랙

    // 프레임 인덱스 계산
    const float FrameTime = Time * FrameRate.AsDecimal();
    const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
    const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
    const float Alpha = FMath::Frac(FrameTime);

    // Spherical Linear Interpolation (Slerp)
    return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& Keys, float Time) const
{
    if (Keys.IsEmpty())
        return FVector(1, 1, 1);

    if (Keys.Num() == 1)
        return Keys[0]; // 상수 트랙

    // 프레임 인덱스 계산
    const float FrameTime = Time * FrameRate.AsDecimal();
    const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
    const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
    const float Alpha = FMath::Frac(FrameTime);

    // 선형 보간
    return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

void UAnimSequence::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // TODO: BoneAnimationTracks 직렬화
    // TODO: FrameRate, NumberOfFrames, NumberOfKeys 직렬화
}
```

---

## 📋 Phase 3: 애니메이션 인스턴스 클래스

### 3.1 UAnimInstance (베이스 클래스)

**파일**: `Source/Runtime/Engine/Animation/AnimInstance.h`

```cpp
#pragma once
#include "Core/Object/Object.h"
#include "AnimationTypes.h"
#include "UAnimInstance.generated.h"

UCLASS(DisplayName="애니메이션 인스턴스", Description="애니메이션 재생 로직")
class UAnimInstance : public UObject
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimInstance() = default;
    virtual ~UAnimInstance() = default;

    // 애니메이션 업데이트 (팀원2가 오버라이드)
    virtual void NativeUpdateAnimation(float DeltaSeconds);

    // Notify 트리거링 (발제 문서 요구사항)
    void TriggerAnimNotifies(float DeltaSeconds);

    // 현재 시간 접근자
    float GetCurrentTime() const { return CurrentTime; }
    void SetCurrentTime(float InTime) { CurrentTime = InTime; }

    // Owner component 접근자
    class USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;

    class USkeletalMeshComponent* OwnerComponent = nullptr;

    friend class USkeletalMeshComponent;
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimInstance.cpp`

```cpp
#include "pch.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    // 기본 구현: 시간만 업데이트
    PreviousTime = CurrentTime;
    CurrentTime += DeltaSeconds;

    // 팀원2가 오버라이드하여 커스텀 로직 구현
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    if (!OwnerComponent)
        return;

    // TODO: 현재 재생 중인 애니메이션의 Notify 체크
    // TODO: PreviousTime ~ CurrentTime 범위의 Notify 트리거
    // TODO: OwnerComponent->HandleAnimNotify() 호출

    // 팀원4가 상세 구현할 예정
}
```

---

### 3.2 UAnimSingleNodeInstance (단일 애니메이션 재생)

**파일**: `Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h`

```cpp
#pragma once
#include "AnimInstance.h"
#include "UAnimSingleNodeInstance.generated.h"

UCLASS(DisplayName="단일 애니메이션 인스턴스", Description="하나의 애니메이션만 재생")
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimSingleNodeInstance() = default;
    virtual ~UAnimSingleNodeInstance() = default;

    // 애니메이션 설정
    void SetAnimationAsset(class UAnimSequence* NewAsset);

    // 재생 제어
    void Play(bool bInLooping = false);
    void Stop();
    void Pause();
    void SetPlayRate(float InPlayRate);

    // 재생 상태 확인
    bool IsPlaying() const { return bIsPlaying; }
    bool IsLooping() const { return bLooping; }
    float GetPlayRate() const { return PlayRate; }

    // 업데이트 구현
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    class UAnimSequence* CurrentSequence = nullptr;
    bool bIsPlaying = false;
    bool bLooping = false;
    float PlayRate = 1.0f;
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimSingleNodeInstance.cpp`

```cpp
#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequence* NewAsset)
{
    CurrentSequence = NewAsset;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;
}

void UAnimSingleNodeInstance::Play(bool bInLooping)
{
    bIsPlaying = true;
    bLooping = bInLooping;

    UE_LOG("UAnimSingleNodeInstance::Play - Looping: %d", bLooping ? 1 : 0);
}

void UAnimSingleNodeInstance::Stop()
{
    bIsPlaying = false;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;

    UE_LOG("UAnimSingleNodeInstance::Stop");
}

void UAnimSingleNodeInstance::Pause()
{
    bIsPlaying = false;

    UE_LOG("UAnimSingleNodeInstance::Pause");
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
    PlayRate = FMath::Max(0.1f, InPlayRate); // 최소 0.1x
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!bIsPlaying || !CurrentSequence)
        return;

    // 시간 업데이트
    PreviousTime = CurrentTime;
    CurrentTime += DeltaSeconds * PlayRate;

    // 애니메이션 길이 체크
    const float AnimLength = CurrentSequence->GetPlayLength();

    if (CurrentTime >= AnimLength)
    {
        if (bLooping)
        {
            // 루핑: 시작으로 돌아가기
            CurrentTime = FMath::Fmod(CurrentTime, AnimLength);
        }
        else
        {
            // 비루핑: 정지
            CurrentTime = AnimLength;
            bIsPlaying = false;

            UE_LOG("UAnimSingleNodeInstance - Animation finished");
        }
    }

    // Notify 트리거
    TriggerAnimNotifies(DeltaSeconds);
}
```

---

## 📋 Phase 4: Component 통합

### 4.1 USkeletalMeshComponent 확장

**수정할 파일**: `Source/Runtime/Engine/Components/SkeletalMeshComponent.h`

**추가할 선언**:

```cpp
// 전방 선언
class UAnimInstance;
class UAnimSequence;
struct FAnimNotifyEvent;
enum class EAnimationMode : uint8;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    // ... 기존 코드 ...

public:
    // 애니메이션 모드
    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="애니메이션 모드")
    EAnimationMode AnimationMode = EAnimationMode::AnimationSingleNode;

    // 애니메이션 인스턴스
    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="애니메이션 인스턴스")
    UAnimInstance* AnimInstance = nullptr;

    // 단일 노드 모드용 애니메이션
    UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="재생할 애니메이션")
    UAnimSequence* AnimationData = nullptr;

    // 재생 제어 (발제 문서 요구사항)
    UFUNCTION(DisplayName="애니메이션_재생", LuaBind)
    void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping = false);

    UFUNCTION(DisplayName="애니메이션_정지", LuaBind)
    void StopAnimation();

    void SetAnimationMode(EAnimationMode InMode);
    void SetAnimation(UAnimSequence* InAnim);
    void Play(bool bLooping);

    // AnimNotify 핸들링 (발제 문서 구조)
    void HandleAnimNotify(const FAnimNotifyEvent& Notify);

protected:
    // TickComponent에서 호출
    void TickAnimation(float DeltaTime);
};
```

**수정할 파일**: `Source/Runtime/Engine/Components/SkeletalMeshComponent.cpp`

**추가할 구현**:

```cpp
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationTypes.h"

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping)
{
    if (!NewAnimToPlay)
    {
        UE_LOG("USkeletalMeshComponent::PlayAnimation - Null animation");
        return;
    }

    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    Play(bLooping);

    UE_LOG("USkeletalMeshComponent::PlayAnimation - %s", NewAnimToPlay->GetName().c_str());
}

void USkeletalMeshComponent::StopAnimation()
{
    if (AnimInstance)
    {
        UAnimSingleNodeInstance* SingleNode = dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance);
        if (SingleNode)
        {
            SingleNode->Stop();
        }
    }

    UE_LOG("USkeletalMeshComponent::StopAnimation");
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
    AnimationMode = InMode;

    // 모드에 맞는 AnimInstance 생성
    if (AnimationMode == EAnimationMode::AnimationSingleNode)
    {
        if (!AnimInstance || !dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance))
        {
            // 새 SingleNode 인스턴스 생성
            AnimInstance = NewObject<UAnimSingleNodeInstance>();
            AnimInstance->OwnerComponent = this;
        }
    }
}

void USkeletalMeshComponent::SetAnimation(UAnimSequence* InAnim)
{
    AnimationData = InAnim;

    UAnimSingleNodeInstance* SingleNode = dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance);
    if (SingleNode)
    {
        SingleNode->SetAnimationAsset(InAnim);
    }
}

void USkeletalMeshComponent::Play(bool bLooping)
{
    UAnimSingleNodeInstance* SingleNode = dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance);
    if (SingleNode)
    {
        SingleNode->Play(bLooping);
    }
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Actor의 HandleAnimNotify 호출 (발제 문서 구조)
        // TODO: AActor에 HandleAnimNotify 가상 함수 추가 필요
        UE_LOG("AnimNotify: %s at time %.2f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
    }
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (!AnimInstance)
        return;

    // 애니메이션 업데이트
    AnimInstance->NativeUpdateAnimation(DeltaTime);

    // 포즈 추출 및 적용
    if (AnimationData)
    {
        FPoseContext Pose;
        FAnimExtractContext Context(AnimInstance->GetCurrentTime(), false);

        AnimationData->GetAnimationPose(Pose, Context);

        // TODO: Pose를 BoneSpaceTransforms에 적용
        // TODO: CPU Skinning 업데이트
    }
}

// TickComponent 수정
void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType)
{
    Super::TickComponent(DeltaTime, TickType);

    // 애니메이션 틱
    TickAnimation(DeltaTime);

    // ... 기존 코드 ...
}
```

---

## 📋 Phase 5: AnimationRuntime 유틸리티

### 5.1 AnimationRuntime 생성

**파일**: `Source/Runtime/Engine/Animation/AnimationRuntime.h`

```cpp
#pragma once
#include "AnimationTypes.h"

// 애니메이션 런타임 유틸리티 (발제 문서 예제)
class FAnimationRuntime
{
public:
    // 두 포즈 블렌딩 (팀원2가 사용할 핵심 함수)
    static void BlendTwoPosesTogether(
        const FPoseContext& PoseA,
        const FPoseContext& PoseB,
        float BlendAlpha,
        FPoseContext& OutPose);

    // 개별 트랜스폼 블렌딩
    static FTransform BlendTransforms(
        const FTransform& A,
        const FTransform& B,
        float Alpha);
};
```

**파일**: `Source/Runtime/Engine/Animation/AnimationRuntime.cpp`

```cpp
#include "pch.h"
#include "AnimationRuntime.h"
#include "Core/Math/Vector.h"

void FAnimationRuntime::BlendTwoPosesTogether(
    const FPoseContext& PoseA,
    const FPoseContext& PoseB,
    float BlendAlpha,
    FPoseContext& OutPose)
{
    // 본 개수 체크
    const int32 NumBones = FMath::Min(PoseA.GetNumBones(), PoseB.GetNumBones());
    OutPose.SetNumBones(NumBones);

    // 각 본의 트랜스폼 블렌딩
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        OutPose.BoneTransforms[BoneIndex] = BlendTransforms(
            PoseA.BoneTransforms[BoneIndex],
            PoseB.BoneTransforms[BoneIndex],
            BlendAlpha
        );
    }
}

FTransform FAnimationRuntime::BlendTransforms(
    const FTransform& A,
    const FTransform& B,
    float Alpha)
{
    // Alpha 클램핑
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // Position: 선형 보간
    FVector BlendedPosition = FMath::Lerp(A.Translation, B.Translation, Alpha);

    // Rotation: Spherical Linear Interpolation (Slerp)
    FQuat BlendedRotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha);

    // Scale: 선형 보간
    FVector BlendedScale = FMath::Lerp(A.Scale3D, B.Scale3D, Alpha);

    return FTransform(BlendedPosition, BlendedRotation, BlendedScale);
}
```

---

## 📋 Phase 6: 빌드 및 검증

### 6.1 Reflection 코드 생성

```bash
python Tools\CodeGenerator\generate.py --source-dir Source\Runtime --output-dir Generated
```

**생성될 파일들**:
- `UAnimationAsset.generated.h/.cpp`
- `UAnimSequenceBase.generated.h/.cpp`
- `UAnimSequence.generated.h/.cpp`
- `UAnimInstance.generated.h/.cpp`
- `UAnimSingleNodeInstance.generated.h/.cpp`

### 6.2 프로젝트 빌드

```bash
msbuild Mundi.sln /p:Configuration=Debug /p:Platform=x64
```

**빌드 성공 확인**:
- [ ] 컴파일 에러 없음
- [ ] 링크 에러 없음
- [ ] 경고 최소화

### 6.3 기본 동작 확인

**테스트 시나리오**:
1. UAnimSequence 인스턴스 생성 가능
2. UAnimInstance 인스턴스 생성 가능
3. USkeletalMeshComponent에 AnimInstance 할당 가능
4. PlayAnimation() 호출 시 크래시 없음

---

## ✅ 완료 기준

### 팀원2 작업 시작 가능 조건

#### 1. 인터페이스 완성
- [x] `UAnimSequence::GetBoneAnimationTracks()` - 애니메이션 데이터 접근
- [x] `UAnimSequence::GetAnimationPose()` - 포즈 추출
- [x] `UAnimInstance` - 상속 가능한 베이스 클래스
- [x] `FPoseContext` - 포즈 데이터 구조
- [x] `FAnimExtractContext` - 애니메이션 추출 컨텍스트
- [x] `FAnimationRuntime::BlendTwoPosesTogether()` - 블렌딩 유틸리티
- [x] `FAnimNotifyEvent` - Notify 시스템 구조
- [x] `USkeletalMeshComponent::PlayAnimation()` - 재생 메서드

#### 2. 빌드 성공
- [ ] 모든 파일 컴파일 성공
- [ ] Reflection 코드 생성 성공
- [ ] 링크 에러 없음

#### 3. 기본 동작 확인
- [ ] UAnimSequence 인스턴스 생성 가능
- [ ] UAnimInstance 인스턴스 생성 가능
- [ ] USkeletalMeshComponent에 AnimInstance 할당 가능

---

## 📝 구현 노트

### 중요 사항

1. **FBX Import는 이번 단계에서 제외**
   - 클래스 구조만 우선 완성
   - FBX 로딩은 Phase 2에서 구현 (Day 3-4)

2. **빈 구현도 OK**
   - `GetAnimationPose()`는 identity transform 반환으로 시작
   - 실제 보간 로직은 기본만 구현

3. **발제 문서 구조 준수**
   - `FBoneAnimationTrack`, `FRawAnimSequenceTrack` 구조 사용
   - `FAnimNotifyEvent` 구조 포함

4. **언리얼 스타일 준수**
   - 클래스 이름: `UAnimSequence`, `FAnimExtractContext`
   - 메서드 이름: `GetAnimationPose()`, `PlayAnimation()`
   - 불리언: `bIsPlaying`, `bLooping`

5. **Reflection 필수**
   - 모든 UCLASS에 `GENERATED_REFLECTION_BODY()` 포함
   - `.generated.h` 파일을 헤더 끝에 include

### 팀원2 작업 시작 시점

**Day 3 완료 시**:
- 위 모든 클래스가 빌드 가능 상태
- 팀원2는 `UAnimInstance`를 상속받아 `NativeUpdateAnimation()` 구현 시작 가능
- 팀원2는 `FAnimationRuntime::BlendTwoPosesTogether()` 사용하여 블렌딩 구현 가능

### 다음 단계 (Day 4-5)

**팀원1이 계속 진행할 작업**:
1. FBX 애니메이션 임포트 구현
2. `UFbxLoader::LoadAnimationFromFbx()` 추가
3. FbxAnimCurve에서 키프레임 추출
4. UAnimSequence 직렬화 완성

**팀원2가 시작할 작업**:
1. `UAnimInstance` 커스텀 구현
2. Animation State Machine 구현
3. Animation Blending 구현
4. `USkeletalMeshComponent`와 통합

---

## 📂 파일 목록 요약

### 생성할 파일 (14개)

**헤더 파일 (7개)**:
1. `Source/Runtime/Engine/Animation/AnimationTypes.h`
2. `Source/Runtime/Engine/Animation/AnimationAsset.h`
3. `Source/Runtime/Engine/Animation/AnimSequenceBase.h`
4. `Source/Runtime/Engine/Animation/AnimSequence.h`
5. `Source/Runtime/Engine/Animation/AnimInstance.h`
6. `Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h`
7. `Source/Runtime/Engine/Animation/AnimationRuntime.h`

**구현 파일 (7개)**:
1. `Source/Runtime/Engine/Animation/AnimationAsset.cpp`
2. `Source/Runtime/Engine/Animation/AnimSequenceBase.cpp`
3. `Source/Runtime/Engine/Animation/AnimSequence.cpp`
4. `Source/Runtime/Engine/Animation/AnimInstance.cpp`
5. `Source/Runtime/Engine/Animation/AnimSingleNodeInstance.cpp`
6. `Source/Runtime/Engine/Animation/AnimationRuntime.cpp`
7. `Source/Runtime/Engine/Components/SkeletalMeshComponent.cpp` (수정)

### 수정할 파일 (2개)
1. `Source/Runtime/Engine/Components/SkeletalMeshComponent.h` - 애니메이션 재생 메서드 추가
2. `Source/Runtime/Engine/Components/SkeletalMeshComponent.cpp` - 구현 추가

### 자동 생성될 파일 (10개)
- `UAnimationAsset.generated.h/.cpp`
- `UAnimSequenceBase.generated.h/.cpp`
- `UAnimSequence.generated.h/.cpp`
- `UAnimInstance.generated.h/.cpp`
- `UAnimSingleNodeInstance.generated.h/.cpp`

---

## 🎯 성공 지표

### Day 3 목표 달성 확인
- [ ] 모든 애니메이션 클래스 빌드 성공
- [ ] 팀원2가 UAnimInstance 상속 가능
- [ ] 팀원2가 FAnimationRuntime 사용 가능
- [ ] 팀원2가 FPoseContext 사용 가능
- [ ] USkeletalMeshComponent에서 PlayAnimation() 호출 가능

### 코드 품질
- [ ] Unreal Engine Coding Standard 준수
- [ ] `UE_LOG` 사용 (std::cout 사용 안 함)
- [ ] `TArray/TMap/TSet` 사용 (std::vector 등 사용 안 함)
- [ ] 리플렉션 시스템 통합 (`UCLASS`, `UPROPERTY`, `UFUNCTION`)
- [ ] 코드 주석 작성 (public 메서드에 설명)

---

**작성자**: 팀원1
**작성일**: 2025-11-13
**버전**: 1.0
**상태**: 작업 준비 완료

이 문서는 Mundi Engine의 애니메이션 시스템 구현을 위한 팀원1의 작업 계획입니다.
팀원2가 Day 3부터 작업을 시작할 수 있도록 애니메이션 클래스 구조를 완성하는 것이 목표입니다.
