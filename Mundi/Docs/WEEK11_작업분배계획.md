# Mundi Engine 애니메이션 시스템 구현 - 4인 팀 작업 분배 계획

## 📊 현재 구현 상태 요약

**전체 진행도: ~15%**

- ✅ **완료**: CPU 스키닝, 스켈레탈 메쉬 기본 구조, FBX 스켈레톤 로딩
- 🟡 **부분 구현**: SkeletalMeshComponent (40%), FBX 인프라 (20%), Skeletal Viewer (30%)
- ❌ **미구현**: 애니메이션 재생, 블렌딩, State Machine, GPU 스키닝, AnimNotify, MiniDump

### 상세 구현 현황표

| 컴포넌트 | 상태 | 완성도 | 담당자 | 우선순위 |
|-----------|--------|------------|------|----------|
| Animation Core Classes | ❌ 미구현 | 0% | 팀원 1 | CRITICAL |
| FBX Animation Import | 🟡 부분 | 20% | 팀원 1 | CRITICAL |
| AnimInstance | ❌ 미구현 | 0% | 팀원 2 | CRITICAL |
| State Machine | ❌ 미구현 | 0% | 팀원 2 | HIGH |
| Animation Blending | ❌ 미구현 | 0% | 팀원 2 | HIGH |
| SkeletalMeshComponent | 🟢 부분 | 40% | 팀원 2 | CRITICAL |
| CPU Skinning | ✅ 완료 | 100% | - | - |
| GPU Skinning | ❌ 미구현 | 0% | 팀원 3 | HIGH |
| Performance Stats | 🟡 부분 | 30% | 팀원 3 | MEDIUM |
| MiniDump System | ❌ 미구현 | 0% | 팀원 3 | LOW |
| Skeletal Viewer | 🟡 기본 | 30% | 팀원 4 | MEDIUM |
| Animation Viewer | ❌ 미구현 | 0% | 팀원 4 | MEDIUM |
| AnimNotify System | ❌ 미구현 | 0% | 팀원 4 | MEDIUM |

---

## 👥 팀원별 작업 분배 (4명 1주 스프린트)

### **팀원 1: 애니메이션 코어 & FBX 임포트** 🎯 우선순위: CRITICAL
**예상 작업량: 40-50시간**

#### 담당 작업
1. **애니메이션 클래스 계층 구조 생성**
   - `UAnimationAsset` (베이스 클래스)
   - `UAnimSequenceBase`
   - `UAnimSequence`
   - `UAnimDataModel` (실제 애니메이션 데이터 저장)
   - `FRawAnimSequenceTrack` (Position/Rotation/Scale 키프레임)
   - `FBoneAnimationTrack` (본별 트랙)

2. **FBX 애니메이션 임포트 구현**
   - `UFbxLoader`에 애니메이션 로딩 메서드 추가
   - `FbxAnimCurve`에서 키프레임 데이터 추출
   - FBX 시간을 엔진 프레임레이트로 변환
   - Position, Rotation(Quaternion), Scale 데이터 저장

3. **애니메이션 에셋 로딩/캐싱**

#### 생성할 파일
```
Mundi/Source/Runtime/Engine/Animation/
  ├─ AnimationAsset.h/.cpp
  ├─ AnimSequenceBase.h/.cpp
  ├─ AnimSequence.h/.cpp
  └─ AnimDataModel.h/.cpp
```

#### 수정할 파일
- `Mundi/Source/Editor/FBXLoader.h/.cpp` (애니메이션 메서드 추가)

#### 구현 예제 구조
```cpp
// AnimDataModel.h
class UAnimDataModel : public UObject
{
    GENERATED_REFLECTION_BODY()

public:
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    float PlayLength;
    FFrameRate FrameRate;
    int32 NumberOfFrames;
    int32 NumberOfKeys;

    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
};

// FRawAnimSequenceTrack
struct FRawAnimSequenceTrack
{
    TArray<FVector3f> PosKeys;   // 위치 키프레임
    TArray<FQuat4f>   RotKeys;   // 회전 키프레임 (Quaternion)
    TArray<FVector3f> ScaleKeys; // 스케일 키프레임
};

// FBoneAnimationTrack
struct FBoneAnimationTrack
{
    FName BoneName;                    // Bone 이름
    FRawAnimSequenceTrack InternalTrack; // 실제 애니메이션 데이터
};
```

#### 의존성
- **다른 팀원이 기다림**: 팀원 2, 4가 이 작업 완료 후 시작 가능
- **완료 목표**: 3일차까지 완료 필수

---

### **팀원 2: 애니메이션 인스턴스 & State Machine** 🎯 우선순위: CRITICAL
**예상 작업량: 45-50시간**

#### 담당 작업
1. **UAnimInstance 구현**
   - 포즈 평가 시스템
   - 시간 추적 및 업데이트
   - AnimNotify 트리거링
   - 애니메이션 그래프 평가

2. **UAnimSingleNodeInstance 구현**
   - 단일 애니메이션 재생
   - 루핑 지원
   - 시간 스크러빙
   - 재생 속도 조절

3. **애니메이션 블렌딩 인프라**
   - `FPoseContext` (포즈 데이터 컨테이너)
   - `FAnimExtractContext` (추출 컨텍스트)
   - `FAnimationRuntime::BlendTwoPosesTogether()` (두 포즈 블렌딩)

4. **Animation State Machine**
   - State 열거형 정의 (Idle, Walk, Run, Fly 등)
   - State 전환 로직
   - Character 이동 상태 감지
   - Pawn/Character와 통합

5. **USkeletalMeshComponent에 애니메이션 재생 통합**
   - `PlayAnimation()`, `Stop()`, `SetAnimation()` 메서드 추가
   - `AnimInstance` 프로퍼티 추가
   - `TickComponent()`에서 애니메이션 평가

#### 생성할 파일
```
Mundi/Source/Runtime/Engine/Animation/
  ├─ AnimInstance.h/.cpp
  ├─ AnimSingleNodeInstance.h/.cpp
  ├─ AnimStateMachine.h/.cpp
  ├─ AnimationRuntime.h/.cpp
  └─ PoseContext.h
```

#### 수정할 파일
- `Mundi/Source/Runtime/Engine/Components/SkeletalMeshComponent.h/.cpp`

#### 구현 예제 구조
```cpp
// AnimInstance.h
class UAnimInstance : public UObject
{
    GENERATED_REFLECTION_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds);
    void TriggerAnimNotifies(float DeltaSeconds);

protected:
    float CurrentTime;
    USkeletalMeshComponent* OwnerComponent;
};

// AnimSingleNodeInstance.h
class UAnimSingleNodeInstance : public UAnimInstance
{
    GENERATED_REFLECTION_BODY()

public:
    void SetAnimationAsset(UAnimationAsset* NewAsset);
    void Play(bool bLooping);
    void Stop();

private:
    UAnimSequence* CurrentSequence;
    bool bIsPlaying;
    bool bLooping;
};

// AnimationRuntime.h
class FAnimationRuntime
{
public:
    static void BlendTwoPosesTogether(
        const FPoseContext& PoseA,
        const FPoseContext& PoseB,
        float BlendAlpha,
        FPoseContext& OutPose);
};

// SkeletalMeshComponent에 추가할 메서드
void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping)
{
    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    Play(bLooping);
}
```

#### State Machine 예제
```cpp
enum class EAnimState
{
    AS_Idle,
    AS_Walk,
    AS_Run,
    AS_Fly,
};

class UAnimationStateMachine : public UObject
{
    GENERATED_REFLECTION_BODY()

public:
    void UpdateState(APawn* Pawn)
    {
        if (Pawn->GetMovementMode() == EMovementMode::Walking)
        {
            FVector Velocity = Pawn->GetVelocity();
            if (Velocity.IsNearlyZero())
                CurrentState = EAnimState::AS_Idle;
            else if (Velocity.Size() <= WalkSpeed)
                CurrentState = EAnimState::AS_Walk;
            else
                CurrentState = EAnimState::AS_Run;
        }
        else if (Pawn->GetMovementMode() == EMovementMode::Flying)
        {
            CurrentState = EAnimState::AS_Fly;
        }
    }

private:
    EAnimState CurrentState;
    float WalkSpeed = 300.0f;
    float RunSpeed = 600.0f;
};
```

#### 의존성
- **의존**: 팀원 1의 애니메이션 클래스 완료 후 시작
- **완료 목표**: 5일차까지 완료

---

### **팀원 3: GPU 스키닝 & 퍼포먼스** 🎯 우선순위: HIGH
**예상 작업량: 40-45시간**

#### 담당 작업
1. **GPU 스키닝 셰이더 구현**
   - `SkeletalMesh.hlsl` 버텍스 셰이더 생성
   - 본 매트릭스 상수 버퍼 (register b6)
   - 셰이더에서 버텍스 블렌딩
   - Normal/Tangent 변환

2. **USkeletalMeshComponent GPU 스키닝 확장**
   - `bUseGPUSkinning` 토글 추가
   - 본 매트릭스 상수 버퍼 생성
   - `UpdateBoneMatrixBuffer()` 메서드
   - CPU/GPU 경로 전환 로직

3. **콘솔 명령어 구현**
   - `SetGPUSkinning 1/0`
   - `SetCPUSkinning 1/0`
   - `CauseCrash` (MiniDump 테스트용)

4. **퍼포먼스 프로파일링**
   - `StatsOverlayD2D`에 "STAT SKINNING" 추가
   - CPU 스키닝 시간 추적
   - GPU 스키닝 시간 추적
   - 본 개수, 버텍스 개수 표시

5. **MiniDump 시스템 구현**
   - `WindowsExceptionHandler` 클래스
   - `UnhandledExceptionFilter` 구현
   - `MiniDumpWriteDump` 래퍼
   - 엔진 초기화에 훅

#### 생성할 파일
```
Mundi/Shaders/Materials/
  └─ SkeletalMesh.hlsl

Mundi/Source/Runtime/Core/HAL/
  └─ WindowsExceptionHandler.h/.cpp
```

#### 수정할 파일
- `Mundi/Source/Runtime/Engine/Components/SkeletalMeshComponent.h/.cpp`
- `Mundi/Source/Runtime/Engine/Components/SkinnedMeshComponent.h/.cpp`
- `Mundi/Source/Slate/StatsOverlayD2D.h/.cpp`
- `Mundi/Source/Slate/Widgets/ConsoleWidget.cpp`

#### GPU 스키닝 셰이더 예제
```hlsl
// SkeletalMesh.hlsl
cbuffer BonesBuffer : register(b6)
{
    float4x4 BoneMatrices[128];
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoord : TEXCOORD0;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHTS;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // GPU Skinning
    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);
    float3 skinnedTangent = float3(0, 0, 0);

    for (int i = 0; i < 4; ++i)
    {
        uint boneIndex = input.BoneIndices[i];
        float weight = input.BoneWeights[i];

        float4x4 boneMatrix = BoneMatrices[boneIndex];

        skinnedPos += weight * mul(float4(input.Position, 1.0), boneMatrix);
        skinnedNormal += weight * mul(float4(input.Normal, 0.0), boneMatrix).xyz;
        skinnedTangent += weight * mul(float4(input.Tangent, 0.0), boneMatrix).xyz;
    }

    output.Position = mul(skinnedPos, ViewProjectionMatrix);
    output.WorldPos = skinnedPos.xyz;
    output.Normal = normalize(skinnedNormal);
    output.Tangent = normalize(skinnedTangent);
    output.TexCoord = input.TexCoord;

    return output;
}
```

#### MiniDump 시스템 예제
```cpp
// WindowsExceptionHandler.h
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

class FWindowsExceptionHandler
{
public:
    static void Initialize();
    static void Shutdown();

private:
    static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo);
    static void WriteMiniDump(EXCEPTION_POINTERS* ExceptionInfo);
};

// 콘솔 명령어
void ConsoleCommand_CauseCrash()
{
    // 의도적으로 크래시 발생
    int* nullPtr = nullptr;
    *nullPtr = 42;  // Access violation
}
```

#### 의존성
- **독립 작업**: 다른 팀원과 병렬 진행 가능
- **완료 목표**: 6일차까지 완료

---

### **팀원 4: 에디터/뷰어 & AnimNotify** 🎯 우선순위: MEDIUM-HIGH
**예상 작업량: 35-40시간**

#### 담당 작업
1. **Animation Sequence Viewer 구현**
   - 타임라인 UI (ImGui 슬라이더)
   - Play/Pause/Stop 버튼
   - 프레임 카운터 표시
   - 시간 스크러빙
   - 루프 토글
   - 재생 속도 조절

2. **AnimNotify 시스템 구현**
   - `FAnimNotifyEvent` 구조체
   - `UAnimSequenceBase`에 `TArray<FAnimNotifyEvent>` 추가
   - `UAnimInstance`에서 `TriggerAnimNotifies()` 구현
   - `HandleAnimNotify()` 체인 (Component → Actor → Character)
   - 에디터에서 타임라인에 Notify 배치 UI

3. **SkeletalViewer 확장**
   - 애니메이션 선택 드롭다운
   - 애니메이션 재생 버튼
   - 본 계층구조 + 애니메이션 영향도 표시
   - 현재 포즈 시각화

4. **선택 사항 - 역재생**
   - 음수 재생 속도 지원
   - `bReversePlay` 플래그

#### 생성할 파일
```
Mundi/Source/Slate/Windows/
  └─ SAnimSequenceViewerWindow.h/.cpp

Mundi/Source/Runtime/Engine/Animation/
  └─ AnimNotify.h/.cpp
```

#### 수정할 파일
- `Mundi/Source/Runtime/Engine/SkeletalViewer/ViewerState.h/.cpp`
- `Mundi/Source/Runtime/Engine/Animation/AnimSequence.h/.cpp`

#### AnimNotify 시스템 예제
```cpp
// AnimNotify.h
struct FAnimNotifyEvent
{
    float TriggerTime;      // 트리거 시간 (초)
    float Duration;         // 지속 시간
    FName NotifyName;       // Notify 이름
    FString NotifyData;     // 추가 데이터
};

// AnimSequenceBase.h에 추가
class UAnimSequenceBase : public UAnimationAsset
{
    GENERATED_REFLECTION_BODY()

public:
    UPROPERTY(Category="[애니메이션]", EditAnywhere)
    TArray<FAnimNotifyEvent> Notifies;

    void GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies);
};

// AnimInstance.h에 추가
void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    float PreviousTime = CurrentTime - DeltaSeconds;
    float CurrentTime = this->CurrentTime;

    TArray<FAnimNotifyEvent> TriggeredNotifies;
    CurrentSequence->GetAnimNotifiesInRange(PreviousTime, CurrentTime, TriggeredNotifies);

    for (const FAnimNotifyEvent& Notify : TriggeredNotifies)
    {
        OwnerComponent->HandleAnimNotify(Notify);
    }
}

// SkeletalMeshComponent.h에 추가
void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(Notify);
    }
}

// Character.h에서 오버라이드
void ACharacter::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    if (Notify.NotifyName == "Footstep")
    {
        PlayFootstepSound();
    }
    else if (Notify.NotifyName == "Shoot")
    {
        TriggerFire();
    }
}
```

#### 타임라인 UI 예제
```cpp
// SAnimSequenceViewerWindow.cpp
void SAnimSequenceViewerWindow::RenderTimeline()
{
    if (!CurrentSequence)
        return;

    float PlayLength = CurrentSequence->GetPlayLength();

    ImGui::Text("Animation: %s", CurrentSequence->GetName());
    ImGui::Text("Frame: %d / %d", CurrentFrame, TotalFrames);

    // 타임라인 슬라이더
    if (ImGui::SliderFloat("Time", &CurrentTime, 0.0f, PlayLength))
    {
        // 스크러빙 시 포즈 업데이트
        AnimInstance->SetCurrentTime(CurrentTime);
    }

    // 재생 컨트롤
    if (ImGui::Button("Play"))
        bIsPlaying = true;
    ImGui::SameLine();
    if (ImGui::Button("Pause"))
        bIsPlaying = false;
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        bIsPlaying = false;
        CurrentTime = 0.0f;
    }

    // 루프 토글
    ImGui::Checkbox("Loop", &bLooping);

    // 재생 속도
    ImGui::SliderFloat("Speed", &PlayRate, -2.0f, 2.0f);

    // Notify 마커 표시
    for (const FAnimNotifyEvent& Notify : CurrentSequence->Notifies)
    {
        float NotifyPosX = (Notify.TriggerTime / PlayLength) * TimelineWidth;
        ImGui::SetCursorPosX(NotifyPosX);
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "▼");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Notify.NotifyName.ToString());
        }
    }
}
```

#### 의존성
- **의존**: 팀원 1, 2의 애니메이션 재생 시스템 완료 후 본격 진행
- **완료 목표**: 7일차까지 완료

---

## 📅 일정 계획 (7일 스프린트)

### **Day 1-2 (Foundation & Design)**
**목표: 설계 완료 및 기반 코드 작성**

#### 팀원 1 (애니메이션 코어)
- [ ] 애니메이션 클래스 계층 구조 설계
- [ ] FBX SDK 애니메이션 API 학습
- [ ] `UAnimationAsset`, `UAnimSequenceBase` 기본 클래스 구현
- [ ] `FRawAnimSequenceTrack`, `FBoneAnimationTrack` 구조체 정의

#### 팀원 2 (애니메이션 인스턴스)
- [ ] `UAnimInstance` 인터페이스 설계
- [ ] 포즈 데이터 구조 (`FPoseContext`) 설계
- [ ] `UAnimSingleNodeInstance` 기본 구조 작성
- [ ] `SkeletalMeshComponent` 확장 계획 수립

#### 팀원 3 (GPU 스키닝)
- [ ] GPU 스키닝 셰이더 설계
- [ ] 본 매트릭스 상수 버퍼 구조 설계
- [ ] 기존 CPU 스키닝 코드 분석
- [ ] `SkeletalMesh.hlsl` 기본 구조 작성

#### 팀원 4 (에디터/뷰어)
- [ ] Animation Viewer UI 목업 설계
- [ ] AnimNotify 데이터 구조 설계
- [ ] 기존 SkeletalViewer 코드 분석
- [ ] 타임라인 UI 프로토타입

### **Day 3-4 (Core Implementation - Critical Path)**
**목표: 핵심 기능 구현 (팀원 1의 작업이 블로커)**

#### 팀원 1 (애니메이션 코어) - ⚠️ CRITICAL
- [ ] `UAnimSequence`, `UAnimDataModel` 완전 구현
- [ ] ✅ **MILESTONE: FBX 애니메이션 임포트 완료**
  - [ ] `UFbxLoader`에 `LoadAnimationFromFbx()` 추가
  - [ ] `FbxAnimCurve` 키프레임 데이터 추출
  - [ ] Position/Rotation/Scale 키 변환
  - [ ] 프레임레이트 변환
- [ ] 테스트 FBX 애니메이션 로드 검증

#### 팀원 2 (애니메이션 인스턴스)
- [ ] `UAnimInstance` 핵심 로직 구현
  - [ ] `NativeUpdateAnimation()` 구현
  - [ ] 시간 추적 로직
- [ ] `UAnimSingleNodeInstance` 구현
  - [ ] `SetAnimationAsset()`
  - [ ] `Play()`, `Stop()` 메서드
  - [ ] 루핑 로직
- [ ] 팀원 1 완료 대기 후 통합 시작

#### 팀원 3 (GPU 스키닝)
- [ ] `SkeletalMesh.hlsl` 버텍스 셰이더 완전 구현
- [ ] 본 매트릭스 상수 버퍼 생성 코드
- [ ] `USkeletalMeshComponent`에 GPU 스키닝 경로 추가
  - [ ] `bUseGPUSkinning` 플래그
  - [ ] `UpdateBoneMatrixBuffer()` 메서드
- [ ] CPU/GPU 전환 로직

#### 팀원 4 (에디터/뷰어)
- [ ] 타임라인 UI 기본 구현 (ImGui)
  - [ ] 시간 슬라이더
  - [ ] Play/Pause/Stop 버튼
  - [ ] 프레임 카운터
- [ ] `FAnimNotifyEvent` 구조체 정의
- [ ] 팀원 2 완료 대기

### **Day 5-6 (Integration & Advanced Features)**
**목표: 시스템 통합 및 고급 기능**

#### 팀원 1 (애니메이션 코어)
- [ ] 애니메이션 에셋 캐싱 시스템
- [ ] 여러 FBX 파일 테스트
- [ ] 팀원 2, 4 통합 지원
- [ ] 버그 수정 및 최적화

#### 팀원 2 (애니메이션 인스턴스) - ⚠️ CRITICAL
- [ ] ✅ **MILESTONE: 애니메이션 재생 시스템 완료**
  - [ ] `SkeletalMeshComponent::PlayAnimation()` 완전 구현
  - [ ] `TickComponent()`에서 애니메이션 평가
  - [ ] 본 트랜스폼 업데이트
- [ ] Animation Blending 구현
  - [ ] `FAnimationRuntime::BlendTwoPosesTogether()`
  - [ ] 두 애니메이션 블렌딩 테스트
- [ ] Animation State Machine 구현
  - [ ] `UAnimationStateMachine` 클래스
  - [ ] State 전환 로직
  - [ ] Character 통합

#### 팀원 3 (GPU 스키닝)
- [ ] GPU 스키닝 디버깅 및 최적화
- [ ] 콘솔 명령어 구현
  - [ ] `SetGPUSkinning 1/0`
  - [ ] `SetCPUSkinning 1/0`
- [ ] 퍼포먼스 통계 구현
  - [ ] `StatsOverlayD2D`에 "STAT SKINNING" 추가
  - [ ] CPU/GPU 시간 측정
  - [ ] 본/버텍스 개수 표시
- [ ] MiniDump 시스템 시작
  - [ ] `WindowsExceptionHandler` 기본 구조

#### 팀원 4 (에디터/뷰어)
- [ ] AnimNotify 시스템 구현
  - [ ] `UAnimSequenceBase`에 Notifies 배열 추가
  - [ ] `TriggerAnimNotifies()` 구현
  - [ ] `HandleAnimNotify()` 체인
- [ ] 타임라인에 Notify 마커 표시
- [ ] Notify 배치 UI
- [ ] SkeletalViewer 확장
  - [ ] 애니메이션 선택 드롭다운
  - [ ] 재생 컨트롤

### **Day 7 (Polish & Testing)**
**목표: 최종 통합, 테스트, 버그 수정**

#### 전체 팀
- [ ] **통합 테스트**
  - [ ] 여러 스켈레탈 메쉬 동시 재생
  - [ ] 복잡한 본 계층구조 (50+ 본)
  - [ ] 애니메이션 블렌딩 검증
  - [ ] State Machine 전환 테스트
  - [ ] GPU vs CPU 퍼포먼스 비교

#### 팀원 1
- [ ] FBX 임포트 엣지 케이스 처리
- [ ] 애니메이션 데이터 검증 로직
- [ ] 문서화

#### 팀원 2
- [ ] Animation Blending 완성도 향상
- [ ] State Machine 추가 상태 구현
- [ ] 버그 수정

#### 팀원 3
- [ ] MiniDump 시스템 완성
  - [ ] `UnhandledExceptionFilter` 구현
  - [ ] `MiniDumpWriteDump` 래퍼
  - [ ] `CauseCrash` 콘솔 명령어
- [ ] GPU 스키닝 최종 최적화
- [ ] 퍼포먼스 통계 완성도

#### 팀원 4
- [ ] 역재생 구현 (선택 사항)
- [ ] 타임라인 UI 완성도 향상
- [ ] AnimNotify 에디터 기능 추가
- [ ] 문서화

---

## ⚠️ 리스크 관리

### 높은 리스크 (High Risk)

#### 1. FBX AnimCurve API 복잡도 (팀원 1)
**문제**: FBX SDK의 애니메이션 커브 API가 복잡하고 문서가 불충분할 수 있음

**완화 전략**:
- Day 1에 FBX SDK 샘플 코드 집중 학습
- Autodesk 공식 문서 및 커뮤니티 참고
- 간단한 테스트 애니메이션으로 먼저 검증
- 필요시 팀원 2가 Day 4부터 지원

**백업 플랜**:
- FBX 대신 간단한 JSON 포맷으로 임시 애니메이션 데이터 생성
- 나중에 FBX 임포트 추가

#### 2. 애니메이션 블렌딩 수학 (팀원 2)
**문제**: Quaternion SLERP 및 포즈 보간이 수학적으로 복잡할 수 있음

**완화 전략**:
- 기존 엔진의 Quaternion 유틸리티 함수 사용
- Unreal Engine 소스 코드 참고
- 선형 보간(LERP)으로 먼저 구현 후 SLERP로 개선

**백업 플랜**:
- 블렌딩 없이 단일 애니메이션 재생만 먼저 완성
- 블렌딩은 선택 기능으로 분류

#### 3. GPU 스키닝 디버깅 난이도 (팀원 3)
**문제**: 셰이더 버그는 CPU 코드보다 디버깅이 어려움

**완화 전략**:
- RenderDoc 또는 PIX for Windows 사용
- CPU 스키닝 결과와 비교하여 검증
- 단계별로 본 하나씩 추가하며 테스트
- 간단한 메쉬(큐브)로 먼저 검증

**백업 플랜**:
- GPU 스키닝을 선택 기능으로 분류
- CPU 스키닝만으로도 기본 요구사항 충족

### 중간 리스크 (Medium Risk)

#### 4. State Machine 통합 (팀원 2)
**문제**: Character/Pawn 시스템과의 통합이 복잡할 수 있음

**완화 전략**:
- 간단한 Idle/Walk 전환부터 시작
- 하드코딩된 조건으로 먼저 구현
- 점진적으로 일반화

#### 5. 타임라인 UI 커스터마이징 (팀원 4)
**문제**: ImGui 기본 위젯으로 전문적인 타임라인 구현이 어려울 수 있음

**완화 전략**:
- ImGui 기본 슬라이더로 시작
- 기능 우선, 비주얼은 나중에 개선
- Notify 마커는 텍스트로 먼저 표시

### 낮은 리스크 (Low Risk)

#### 6. AnimNotify 시스템 (팀원 4)
**문제**: 비교적 단순한 이벤트 시스템

**완화 전략**:
- 표준 이벤트 패턴 사용
- 검증된 델리게이트 패턴

#### 7. MiniDump 시스템 (팀원 3)
**문제**: Windows API는 잘 문서화되어 있음

**완화 전략**:
- MSDN 문서 참고
- 샘플 코드 풍부

---

## ✅ 테스트 전략

### Phase 1: 핵심 기능 검증 (Day 3-4)

#### 테스트 케이스
1. **FBX 애니메이션 로드**
   - [ ] 간단한 큐브 회전 애니메이션 로드
   - [ ] 복잡한 캐릭터 걷기 애니메이션 로드
   - [ ] 키프레임 데이터 정확성 검증
   - [ ] 프레임레이트 변환 검증

2. **단일 애니메이션 재생**
   - [ ] Play/Stop 동작 확인
   - [ ] 루핑 동작 확인
   - [ ] 시간 스크러빙 확인
   - [ ] 본 트랜스폼 업데이트 확인

3. **시각적 검증**
   - [ ] 애니메이션이 부드럽게 재생되는지 확인
   - [ ] 본이 올바르게 변형되는지 확인
   - [ ] 메쉬가 올바르게 따라가는지 확인

#### 성공 기준
- 모든 키프레임이 정확하게 추출됨
- 애니메이션이 끊김 없이 재생됨
- 본 트랜스폼이 예상대로 업데이트됨

### Phase 2: 고급 기능 검증 (Day 5-6)

#### 테스트 케이스
1. **애니메이션 블렌딩**
   - [ ] Idle → Walk 블렌딩 (0.0 → 1.0)
   - [ ] 중간 BlendAlpha 값 (0.5)에서 포즈 확인
   - [ ] 블렌딩 속도 조절
   - [ ] 여러 애니메이션 체인 블렌딩

2. **State Machine**
   - [ ] Idle 상태에서 시작
   - [ ] 이동 시 Walk 전환
   - [ ] 빠른 이동 시 Run 전환
   - [ ] 정지 시 Idle 복귀
   - [ ] Flying 모드 전환

3. **GPU vs CPU 스키닝**
   - [ ] CPU 스키닝으로 캐릭터 재생
   - [ ] GPU 스키닝으로 전환
   - [ ] 시각적 차이 확인 (없어야 함)
   - [ ] 퍼포먼스 측정 (GPU가 더 빨라야 함)
   - [ ] 여러 캐릭터 동시 재생 (10개 이상)

4. **AnimNotify**
   - [ ] 특정 시간에 Notify 트리거 확인
   - [ ] 콘솔 로그로 트리거 확인
   - [ ] Character의 HandleAnimNotify 호출 확인
   - [ ] 여러 Notify 동시 처리

#### 성공 기준
- 블렌딩이 자연스럽게 보임
- State 전환이 즉각적으로 반응
- GPU 스키닝이 CPU보다 빠름
- Notify가 정확한 시간에 트리거됨

### Phase 3: 에디터 & UI 검증 (Day 6-7)

#### 테스트 케이스
1. **Animation Viewer**
   - [ ] 타임라인 슬라이더로 스크러빙
   - [ ] Play 버튼으로 재생
   - [ ] Pause 버튼으로 일시정지
   - [ ] Stop 버튼으로 정지 및 리셋
   - [ ] 루프 토글 동작 확인
   - [ ] 재생 속도 조절 (0.5x, 2.0x)

2. **Notify 에디터**
   - [ ] 타임라인에 Notify 마커 표시
   - [ ] 마커 호버 시 Notify 이름 표시
   - [ ] Notify 추가/삭제 (선택 기능)

3. **역재생 (선택)**
   - [ ] 음수 재생 속도 (-1.0)
   - [ ] 역방향 루핑
   - [ ] 역방향 스크러빙

#### 성공 기준
- UI가 직관적이고 반응적임
- 모든 버튼이 예상대로 동작
- Notify 시각화가 명확함

### Phase 4: 안정성 & 퍼포먼스 검증 (Day 7)

#### 테스트 케이스
1. **MiniDump 시스템**
   - [ ] `CauseCrash` 콘솔 명령어 실행
   - [ ] `.dmp` 파일 생성 확인
   - [ ] Visual Studio에서 `.dmp` 파일 열기
   - [ ] 콜스택 확인
   - [ ] `.pdb` 심볼 파일과 연동 확인

2. **복잡한 시나리오**
   - [ ] 50개 이상의 본을 가진 캐릭터
   - [ ] 10개 이상의 캐릭터 동시 재생
   - [ ] 긴 애니메이션 (60초 이상)
   - [ ] 여러 애니메이션 빠르게 전환

3. **퍼포먼스 측정**
   - [ ] STAT SKINNING 명령어로 통계 확인
   - [ ] CPU 스키닝 시간 측정
   - [ ] GPU 스키닝 시간 측정
   - [ ] 본 개수별 퍼포먼스 차이
   - [ ] 캐릭터 개수별 퍼포먼스 차이

4. **엣지 케이스**
   - [ ] 애니메이션이 없는 메쉬
   - [ ] 키프레임이 하나만 있는 애니메이션
   - [ ] 매우 빠른 재생 속도 (10.0x)
   - [ ] 매우 느린 재생 속도 (0.1x)
   - [ ] 애니메이션 재생 중 메쉬 교체

#### 성공 기준
- 크래시 없이 모든 시나리오 동작
- MiniDump가 정확한 크래시 정보 포함
- GPU 스키닝이 CPU보다 최소 2배 이상 빠름
- 10개 캐릭터 동시 재생 시 60 FPS 유지

---

## 📚 학습 리소스

### FBX SDK (팀원 1)
- [Autodesk FBX SDK Programmer's Guide](https://help.autodesk.com/view/FBX/2020/ENU/)
- [FBX SDK - Animation](https://help.autodesk.com/view/FBX/2020/ENU/?guid=FBX_Developer_Help_nodes_and_scene_graph_fbx_scenes_animation_html)
- FBX SDK 샘플 코드: `C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.7\samples\`

### Unreal Engine 소스 (팀원 2)
- `Engine/Source/Runtime/Engine/Classes/Animation/AnimSequence.h`
- `Engine/Source/Runtime/Engine/Classes/Animation/AnimInstance.h`
- `Engine/Source/Runtime/Engine/Private/Animation/AnimationRuntime.cpp`
- [Unreal Engine Animation System](https://docs.unrealengine.com/5.0/en-US/animation-system-in-unreal-engine/)

### GPU 프로그래밍 (팀원 3)
- [GPU Gems 3 - Chapter 3: Skinning](https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-3-directx-10-blend-shapes-skinning)
- [MSDN - Direct3D 11 Compute Shader](https://docs.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader)
- [Skeletal Animation on GPU - SlideShare](https://www.slideshare.net/slideshow/skeletal-animation-on-gpu/14461701)

### ImGui (팀원 4)
- [ImGui GitHub](https://github.com/ocornut/imgui)
- [ImGui Demo Window](https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp) - 모든 위젯 예제
- [Custom ImGui Widgets](https://github.com/ocornut/imgui/wiki/Useful-Extensions)

### Windows 디버깅 (팀원 3)
- [MiniDumpWriteDump Function](https://docs.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump)
- [Debugging with MiniDumps](https://docs.microsoft.com/en-us/visualstudio/debugger/using-dump-files)

### 핵심 개념

#### Quaternion과 SLERP (팀원 2)
- Quaternion은 회전을 표현하는 4D 벡터 (w, x, y, z)
- SLERP (Spherical Linear Interpolation): 두 회전 사이의 부드러운 보간
- 엔진의 `FQuat::Slerp()` 함수 사용

#### Linear Blend Skinning (LBS)
- 현재 CPU 스키닝 구현에 사용 중
- 각 버텍스가 최대 4개의 본에 영향을 받음
- 가중치 합이 1.0이어야 함
- 공식: `FinalPos = Σ(weight_i * BoneMatrix_i * VertexPos)`

#### 애니메이션 압축 (선택 학습)
- 키프레임 리덕션
- Quaternion 압축
- Curve 압축
- 메모리 vs 품질 트레이드오프

---

## 💡 협업 전략

### 일일 스탠드업 미팅 (매일 오전 10:00, 15분)

#### 포맷
1. **어제 완료한 작업** (각 2분)
2. **오늘 계획** (각 2분)
3. **블로커 및 도움 요청** (각 1분)

#### 예시
```
팀원 1:
- 어제: UAnimSequence 클래스 구조 완성, FBX SDK 문서 학습
- 오늘: FbxAnimCurve에서 키프레임 추출 코드 작성
- 블로커: FBX 시간 단위 변환 방법이 불명확함 → 팀원 3에게 조언 요청

팀원 2:
- 어제: UAnimInstance 인터페이스 설계 완료
- 오늘: NativeUpdateAnimation() 구현 시작
- 블로커: 팀원 1의 UAnimSequence 완성 대기 중

팀원 3:
- 어제: GPU 스키닝 셰이더 기본 구조 작성
- 오늘: 본 매트릭스 버퍼 바인딩 코드
- 블로커: 없음

팀원 4:
- 어제: ImGui 타임라인 프로토타입
- 오늘: 재생 컨트롤 버튼 추가
- 블로커: 팀원 2의 AnimInstance 완성 필요 (Day 5부터)
```

### 코드 리뷰 전략

#### Pair Programming (권장)
- **팀원 1 ↔ 팀원 2**: 애니메이션 코어 시스템
  - Day 3-4: 팀원 2가 팀원 1의 FBX 코드 리뷰
  - Day 5-6: 팀원 1이 팀원 2의 AnimInstance 코드 리뷰

- **팀원 3 ↔ 팀원 4**: 렌더링 및 에디터
  - Day 4-5: 팀원 4가 팀원 3의 셰이더 코드 리뷰
  - Day 6-7: 팀원 3이 팀원 4의 UI 코드 리뷰

#### Pull Request 리뷰 (필수)
- 모든 기능 완료 시 PR 생성
- 최소 1명의 승인 필요
- Critical 기능은 2명의 승인 필요

### 통합 체크포인트

#### Checkpoint 1: Day 3 (애니메이션 클래스 완료)
**목표**: 팀원 1의 마일스톤 달성 확인

**체크리스트**:
- [ ] UAnimSequence 클래스가 컴파일됨
- [ ] FBX 애니메이션 파일을 로드할 수 있음
- [ ] 키프레임 데이터가 올바르게 추출됨
- [ ] 팀원 2가 이 클래스를 사용할 수 있음

**문제 발생 시**:
- 팀원 3이 팀원 1 지원
- 팀원 2는 더미 데이터로 AnimInstance 개발 계속

#### Checkpoint 2: Day 5 (재생 시스템 완료)
**목표**: 팀원 2의 마일스톤 달성 확인

**체크리스트**:
- [ ] 애니메이션이 실제로 재생됨
- [ ] Play/Stop 동작함
- [ ] 본이 올바르게 움직임
- [ ] 메쉬가 본을 따라감

**문제 발생 시**:
- 전체 팀이 팀원 2 지원
- Day 6까지 완료를 목표로 조정

#### Checkpoint 3: Day 7 (최종 통합)
**목표**: 모든 기능 통합 및 테스트

**체크리스트**:
- [ ] 모든 필수 기능 동작함
- [ ] 통합 테스트 통과
- [ ] 알려진 크래시 없음
- [ ] 문서화 완료

### Git 브랜치 전략

#### 브랜치 구조
```
main (보호됨)
├─ feature/animation-core (팀원 1)
├─ feature/animation-instance (팀원 2)
├─ feature/gpu-skinning (팀원 3)
└─ feature/animation-viewer (팀원 4)
```

#### 브랜치 규칙
1. **직접 main에 푸시 금지**
2. **feature 브랜치는 매일 main과 동기화**
3. **PR은 최소 1명의 승인 필요**
4. **빌드 성공한 코드만 merge**

#### Commit Message 규칙
```
[Category] Brief description

- Detailed change 1
- Detailed change 2

Related: WEEK11_발제.md
```

**예시**:
```
[Animation] Implement FBX animation import

- Add LoadAnimationFromFbx() to UFbxLoader
- Extract keyframes from FbxAnimCurve
- Convert FBX time to engine frame rate

Related: WEEK11_발제.md
```

**Categories**:
- `[Animation]`: 애니메이션 시스템
- `[Rendering]`: 렌더링 (GPU 스키닝)
- `[Editor]`: 에디터/뷰어 UI
- `[Core]`: 코어 시스템 (MiniDump 등)
- `[Fix]`: 버그 수정
- `[Test]`: 테스트 코드

### 커뮤니케이션

#### 도구
- **Discord/Slack**: 실시간 커뮤니케이션
- **GitHub Issues**: 버그 추적
- **GitHub Projects**: 작업 보드
- **공유 문서**: 기술 문서, 회의록

#### 채널 구성 (Discord/Slack)
- `#general`: 일반 대화
- `#standup`: 일일 스탠드업 기록
- `#animation-core`: 팀원 1, 2 논의
- `#rendering-editor`: 팀원 3, 4 논의
- `#blockers`: 긴급 블로커 공유
- `#resources`: 유용한 링크, 문서

---

## 🎯 완료 기준 (Definition of Done)

### 필수 기능 (Must Have) - 모두 완료되어야 함

#### 1. FBX 애니메이션 임포트 ✅
- [ ] FBX 파일에서 애니메이션 커브 로드
- [ ] Position/Rotation/Scale 키프레임 추출
- [ ] 프레임레이트 변환
- [ ] `UAnimSequence` 에셋 생성

**검증 방법**:
- 테스트 FBX 파일 로드
- 키프레임 개수가 일치
- 애니메이션 길이가 정확

#### 2. 단일 애니메이션 재생 ✅
- [ ] `PlayAnimation()` 메서드 구현
- [ ] Play/Stop/Pause 기능
- [ ] 루핑 지원
- [ ] 본 트랜스폼 업데이트

**검증 방법**:
- 캐릭터가 부드럽게 움직임
- 루프 애니메이션이 끊김 없이 반복
- Stop 시 초기 포즈로 복귀

#### 3. GPU 스키닝 ✅
- [ ] `SkeletalMesh.hlsl` 셰이더 구현
- [ ] 본 매트릭스 상수 버퍼
- [ ] CPU/GPU 전환 콘솔 명령어
- [ ] CPU와 동일한 시각적 결과

**검증 방법**:
- `SetGPUSkinning 1` 실행
- 캐릭터가 CPU와 동일하게 렌더링
- 퍼포먼스가 더 빠름

#### 4. 타임라인 UI ✅
- [ ] ImGui 타임라인 슬라이더
- [ ] Play/Pause/Stop 버튼
- [ ] 프레임 카운터
- [ ] 시간 스크러빙

**검증 방법**:
- 슬라이더로 애니메이션 스크러빙
- 버튼이 모두 동작
- 프레임 번호가 정확

#### 5. MiniDump 시스템 ✅
- [ ] `WindowsExceptionHandler` 구현
- [ ] 크래시 시 `.dmp` 파일 생성
- [ ] `CauseCrash` 콘솔 명령어
- [ ] Visual Studio에서 `.dmp` 열기 가능

**검증 방법**:
- `CauseCrash` 실행
- `.dmp` 파일 생성 확인
- Visual Studio에서 콜스택 확인

### 권장 기능 (Should Have) - 가능한 한 완료

#### 6. Animation Blending ⭐
- [ ] `FAnimationRuntime::BlendTwoPosesTogether()` 구현
- [ ] 두 애니메이션 블렌딩
- [ ] BlendAlpha 파라미터

**검증 방법**:
- Idle과 Walk 블렌딩
- BlendAlpha 0.5에서 중간 포즈

#### 7. Animation State Machine ⭐
- [ ] `UAnimationStateMachine` 클래스
- [ ] Idle/Walk/Run 상태
- [ ] 자동 상태 전환

**검증 방법**:
- 캐릭터 이동 시 자동 전환
- 상태가 올바르게 변경됨

#### 8. AnimNotify ⭐
- [ ] `FAnimNotifyEvent` 구조체
- [ ] Notify 트리거링
- [ ] `HandleAnimNotify()` 체인

**검증 방법**:
- 특정 프레임에 Notify 트리거
- 콘솔 로그 확인

#### 9. 퍼포먼스 통계 ⭐
- [ ] `STAT SKINNING` 콘솔 명령어
- [ ] CPU/GPU 시간 표시
- [ ] 본/버텍스 개수 표시

**검증 방법**:
- `STAT SKINNING` 실행
- 통계가 화면에 표시됨

### 선택 기능 (Nice to Have) - 시간이 남으면 구현

#### 10. 역재생 (Reverse Play) ⚡
- [ ] 음수 재생 속도
- [ ] `bReversePlay` 플래그

#### 11. Lua State Machine ⚡
- [ ] Lua 스크립트로 State Machine 정의
- [ ] 런타임에 로드

#### 12. 고급 타임라인 UI ⚡
- [ ] 커스텀 타임라인 위젯
- [ ] 드래그 앤 드롭 Notify 배치
- [ ] 줌/패닝

---

## 📊 작업량 추정 요약

| 팀원 | 주요 작업 | 예상 시간 | 리스크 | 완료 목표일 |
|------|-----------|----------|--------|------------|
| **팀원 1** | 애니메이션 코어 & FBX 임포트 | 40-50h | 높음 | Day 3 |
| **팀원 2** | AnimInstance & State Machine | 45-50h | 높음 | Day 5 |
| **팀원 3** | GPU 스키닝 & 퍼포먼스 | 40-45h | 중간 | Day 6 |
| **팀원 4** | 에디터/뷰어 & AnimNotify | 35-40h | 낮음 | Day 7 |
| **전체** | **통합 및 테스트** | **160-185h** | | **Day 7** |

**1인당 평균**: 40-46시간 (1주 40시간 + 최대 6시간 오버타임)

---

## 📝 문서화 요구사항

### 코드 문서화
- 모든 public 메서드에 주석 (한글 또는 영어)
- 복잡한 알고리즘에 설명 추가
- TODO/FIXME 태그 사용

### 기술 문서
- 각 팀원이 자신의 작업 영역 문서화
- Markdown 형식 (`Mundi/Docs/`)
- 아키텍처 다이어그램 (선택)

### 사용자 가이드
- 콘솔 명령어 목록
- 애니메이션 임포트 가이드
- Viewer 사용법

---

## 🚀 최종 체크리스트 (Day 7 완료 시)

### 기능 체크리스트
- [ ] 모든 필수 기능 (Must Have) 완료
- [ ] 최소 2개의 권장 기능 (Should Have) 완료
- [ ] 통합 테스트 통과
- [ ] 알려진 Critical 버그 없음

### 코드 품질
- [ ] 모든 코드가 Unreal Engine Coding Standard 준수
- [ ] `UE_LOG` 사용 (std::cout 사용 안 함)
- [ ] `TArray/TMap/TSet` 사용 (std::vector 등 사용 안 함)
- [ ] 리플렉션 시스템 통합 (`UCLASS`, `UPROPERTY`)
- [ ] 코드 리뷰 완료
- [ ] 컴파일 경고 없음

### 테스트
- [ ] Phase 1 테스트 통과
- [ ] Phase 2 테스트 통과
- [ ] Phase 3 테스트 통과
- [ ] Phase 4 테스트 통과

### 문서
- [ ] 코드 주석 작성
- [ ] 기술 문서 작성
- [ ] README 업데이트
- [ ] 알려진 이슈 문서화

### Git
- [ ] 모든 feature 브랜치 merge
- [ ] main 브랜치 빌드 성공
- [ ] 모든 PR 승인 및 merge
- [ ] 커밋 메시지 일관성

---

## 🎓 학습 목표 달성 체크리스트 (WEEK11_발제.md 기준)

- [ ] FBX Anim Curve 정보를 이해한다.
- [ ] Bone Animation을 이해한다.
- [ ] Animation Blending을 이해한다.
- [ ] Skeleton, SkeletalMesh, AnimSequence 클래스의 역할을 이해한다.
- [ ] Animation Sequence Viewer의 역할을 이해한다.
- [ ] Animation State Machine을 구현한다.
- [ ] CPU와 GPU Skinning의 차이를 이해하고 구현한다.
- [ ] MiniDump를 생성하고 `*.dmp` 파일과 `*.pdb`의 관계를 이해한다.

---

**작성일**: 2025-11-13
**작성자**: Claude Code
**버전**: 1.0
**상태**: 초안

---

## 변경 이력

| 날짜 | 버전 | 변경 내용 | 작성자 |
|------|------|----------|--------|
| 2025-11-13 | 1.0 | 초안 작성 | Claude Code |

---

**이 문서는 WEEK11 애니메이션 시스템 구현을 위한 4인 팀의 작업 분배 계획입니다.**
**실제 진행 상황에 따라 계획을 유연하게 조정하세요.**
