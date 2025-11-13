# Animation System Implementation TODO

## 핵심 학습 목표

- [ ] FBX Anim Curve 정보를 이해한다.
- [ ] Bone Animation을 이해한다.
- [ ] Animation Blending을 이해한다.
- [ ] Skeleton, SkeletalMesh, AnimSequence 클래스의 역할을 이해한다.
- [ ] Animation Sequence Viewer의 역할을 이해한다.
- [ ] Animation State Machine을 구현한다.
- [ ] CPU와 GPU Skinning의 차이를 이해하고 구현한다.
- [ ] MiniDump를 생성하고 `*.dmp` 파일과 `*.pdb`의 관계를 이해한다.

---

## Editor & Rendering (눈에 보이는 세상)

### Animation Sequence Viewer 구현

- [ ] 타임 라인(재생 컨트롤)을 구현한다.
- [ ] Anim Notify를 구현한다.

#### Anim Notify 구조

```cpp
struct FAnimNotifyEvent
{
    float TriggerTime;
    float Duration;
    FName NotifyName;
};

class UAnimSequenceBase
{
    TArray<struct FAnimNotifyEvent> Notifies;
    ...
};

class UAnimInstance
{
    void TriggerAnimNotifies(float DeltaSeconds);
    ...
};

class USkeletalMeshComponent
{
    void HandleAnimNotify(const FAnimNotifyEvent& Notify)
    {
        Owner->HandleAnimNotify(Notify);
    }
};

class ACharacter : public APawn
{
    void HandleAnimNotify(const FAnimNotifyEvent& Notify)
    {
        switch(Notify.NotifyName)
        {
            case NAME_Shoot:
                TriggerFire();
                break;
        }
    }
};
```

### Animation Blending 구현

```cpp
class UMyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UAnimSequence* AnimA;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UAnimSequence* AnimB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlendAlpha = 0.0f;

protected:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    if (AnimA && AnimB)
    {
        FPoseContext PoseA(this);
        FPoseContext PoseB(this);

        FAnimExtractContext ExtractA(GetCurrentTime() /* or custom time */, false);
        FAnimExtractContext ExtractB(GetCurrentTime(), false);

        AnimA->GetAnimationPose(PoseA, ExtractA);
        AnimB->GetAnimationPose(PoseB, ExtractB);

        FAnimationRuntime::BlendTwoPosesTogether(
            PoseA.Pose, PoseB.Pose,
            PoseA.Curve, PoseB.Curve,
            BlendAlpha, /*Out*/ PoseA.Pose, /*Out*/ PoseA.Curve);

        // 결과 포즈를 출력
        Output.Pose = PoseA.Pose;
        Output.Curve = PoseA.Curve;
    }
}
```

---

## Engine Core (눈에 안 보이는 세상)

### FBX 키 애니메이션 정보 읽기

FBX에서 키 애니메이션 (Key Animation) 정보를 읽어 들여 자체 엔진 구조로 변환한다.

**FBX 관련 클래스:**
- `FbxAnimCurve`
- `FbxTime`
- `FbxAnimLayer`
- `FbxAnimStack`

**엔진 내부 구조:**

```cpp
struct FRawAnimSequenceTrack
{
    TArray<FVector3f> PosKeys;   // 위치 키프레임
    TArray<FQuat4f>   RotKeys;   // 회전 키프레임 (Quaternion)
    TArray<FVector3f> ScaleKeys; // 스케일 키프레임
};

struct FBoneAnimationTrack
{
    FName Name;                        // Bone 이름
    FRawAnimSequenceTrack InternalTrack; // 실제 애니메이션 데이터
};
```

### Animation Sequence 클래스 구현

```cpp
class UAnimDataModel : public UObject
{
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    float PlayLength;
    FFrameRate FrameRate;
    int32 NumberOfFrames;
    int32 NumberOfKeys;
    FAnimationCurveData CurveData;

    virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override;
    ...
};

class UAnimSequenceBase : public UAnimationAsset
{
    UAnimDataModel* GetDataMode() const;
    ...
};

class UAnimSequence : public UAnimSequenceBase
{
    ...
};

void USkeletalMeshComponent::PlayAnimation(class UAnimationAsset* NewAnimToPlay, bool bLooping)
{
    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    Play(bLooping);
}
```

### Animation State Machine 구현

```cpp
enum EAnimState
{
    AS_Idle,
    AS_Work,
    AS_Run,
    AS_Fly,
    ...
};

class UAnimationStateMachine : public UObject
{
    ...

    void ProcessState()
    {
        if (Pawn->GetMovementMode() == MM_Walking)
        {
            if (Pawn->GetVelocity().IsNearlyZero())
            {
                AS_Idle;
            }
            else if(Pawn->GetVelocity() <= Pawn->GetMovementComponent()->GetWorkSpeed())
            {
                AS_Work;
            }
            else if(Pawn->GetVelocity() <= Pawn->GetMovementComponent()->GetRunSpeed())
            {
                AS_Run;
            }
        }
        else if(Pawn->GetMovementMode() == MM_Flying)
        {
            AS_Fly;
        }
    }
};
```

### Animation Instance 클래스 구현

```cpp
class UAnimInstance : public UObject
{
    // UAnimationStatetMachine* AnimSM;
};

class UAnimSingleNodeInstance : public UAnimInstance
{
};
```

### GPU Skinning 구현

**요구사항:**
- CPU와 GPU Skinning 비교를 위한 Performance Profile용 Stat을 구현한다.
- CPU와 GPU Skinning을 콘솔 명령어로 전환 가능하다.

```hlsl
// HLSL Shader 예제
cbuffer BonesBuffer : register(b0)
{
    float4x4 boneMatrices[128];
};

VertexOutput main(VertexInput IN)
{
    for (int i = 0; i < 4; ++i)
    {
        uint boneIndex = IN.boneIndices[i];
        float weight = IN.boneWeights[i];
        ...
    }
}
```

### MiniDump 시스템 구현

**요구사항:**
- 엔진 실행 중 임의의 위치(CallStack)에서 Crash를 일으키는 콘솔 명령어를 구현한다. (`CauseCrash`)

```cpp
#include <dbghelp.h>
#include <exception>

#pragma comment(lib, "dbghelp.lib")

// 주요 함수:
// - MiniDumpWriteDump()
// - RaiseException()
// - SetUnhandledExceptionFilter()

try
{
}
catch (const std::exception& e)
{
}
catch (...)
{
}
```

---

## 선택 학습

### Reverse Play Animation 구현

`bReversePlay` 또는 negative play rate (`-1`)으로 조절한다.

### Animation State Machine을 Lua로 구현

C++ 코드 대신 Lua Script로 구현한다.

### Unreal Property Reflection System

추가 학습 항목
