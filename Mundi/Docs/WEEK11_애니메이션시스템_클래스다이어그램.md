# Mundi Engine 애니메이션 시스템 클래스 다이어그램

**작성일**: 2025-11-14
**버전**: 1.0

---

## 전체 클래스 다이어그램

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                        MUNDI ENGINE ANIMATION SYSTEM                         ║
║                              CLASS DIAGRAM                                   ║
╚══════════════════════════════════════════════════════════════════════════════╝

┌─────────────────────────────────────────────────────────────────────────────┐
│                          ASSET HIERARCHY (에셋 계층)                         │
└─────────────────────────────────────────────────────────────────────────────┘

                    ┌──────────────────────┐
                    │   UResourceBase      │  (엔진 베이스)
                    │──────────────────────│
                    │ + Serialize()        │
                    │ + GetName()          │
                    └──────────┬───────────┘
                               │
                               │ ◁───── 상속 (inheritance)
                               │
                    ┌──────────▼───────────┐
                    │  UAnimationAsset     │  (추상)
                    │──────────────────────│
                    │ + Skeleton: USkeleton*          ◆────┐
                    │ + MetaData: TArray<UAnimMetaData*>   │
                    │──────────────────────│               │  참조
                    │ + GetPlayLength(): float (virtual)   │  (association)
                    │ + Serialize()        │               │
                    └──────────┬───────────┘               │
                               │                            │
                               │ ◁───── 상속               │
                               │                            │
                    ┌──────────▼───────────┐               │
                    │ UAnimSequenceBase    │  (추상)      │
                    │──────────────────────│               │
                    │ + Notifies: TArray<FAnimNotifyEvent> ◆─┐
                    │ + SequenceLength: float              │  │
                    │ + RateScale: float   │               │  │
                    │──────────────────────│               │  │
                    │ + GetAnimationPose() (pure virtual)  │  │
                    │ + GetAnimNotifiesInRange()           │  │
                    │ + GetPlayLength()    │               │  │
                    └──────────┬───────────┘               │  │
                               │                            │  │
                               │ ◁───── 상속               │  │
                               │                            │  │
                    ┌──────────▼───────────┐               │  │
                    │   UAnimSequence      │  (구체)      │  │
                    │──────────────────────│               │  │
                    │ + FrameRate: FFrameRate             ◆──┼─┼─┐
                    │ + NumberOfFrames: int32             │  │  │ │
                    │ + NumberOfKeys: int32               │  │  │ │
                    │ - BoneAnimationTracks: TArray<>    ◆──┼─┼─┼─┐
                    │──────────────────────│               │  │  │ │
                    │ + GetAnimationPose() │  (구현)      │  │  │ │
                    │ + GetBoneTransformAtTime()          │  │  │ │
                    │ + AddBoneTrack()     │               │  │  │ │
                    │ + InterpolatePosition()             │  │  │ │
                    │ + InterpolateRotation()             │  │  │ │
                    │ + InterpolateScale() │               │  │  │ │
                    └──────────────────────┘               │  │  │ │
                                                           │  │  │ │
┌──────────────────────────────────────────────────────────┼──┼──┼─┼──┐
│                    RUNTIME HIERARCHY (런타임 계층)       │  │  │ │  │
└──────────────────────────────────────────────────────────┼──┼──┼─┼──┘
                                                           │  │  │ │
                    ┌──────────────────────┐               │  │  │ │
                    │      UObject         │  (엔진 베이스)│  │  │ │
                    │──────────────────────│               │  │  │ │
                    │ + GetName()          │               │  │  │ │
                    │ + GetClass()         │               │  │  │ │
                    └──────────┬───────────┘               │  │  │ │
                               │                            │  │  │ │
                               │ ◁───── 상속               │  │  │ │
                               │                            │  │  │ │
                    ┌──────────▼───────────┐               │  │  │ │
                    │   UAnimInstance      │  (추상)      │  │  │ │
                    │──────────────────────│               │  │  │ │
                    │ # CurrentTime: float │               │  │  │ │
                    │ # PreviousTime: float│               │  │  │ │
                    │ # OwnerComponent: USkeletalMeshComponent* ◀─┼─┼─┼─┼─┐
                    │──────────────────────│               │  │  │ │ │
                    │ + NativeUpdateAnimation() (virtual)  │  │  │ │ │
                    │ + TriggerAnimNotifies()              │  │  │ │ │
                    │ + GetCurrentTime()   │               │  │  │ │ │
                    │ + GetOwnerComponent()│               │  │  │ │ │
                    └──────────┬───────────┘               │  │  │ │ │
                               │                            │  │  │ │ │
                               │ ◁───── 상속               │  │  │ │ │
                               │                            │  │  │ │ │
            ┌──────────────────▼────────────────┐          │  │  │ │ │
            │  UAnimSingleNodeInstance          │  (구체) │  │  │ │ │
            │───────────────────────────────────│          │  │  │ │ │
            │ - CurrentSequence: UAnimSequence* │ ◀────────┼──┼──┼─┼─┘
            │ - bIsPlaying: bool                │          │  │  │ │
            │ - bLooping: bool                  │          │  │  │ │
            │ - PlayRate: float                 │          │  │  │ │
            │───────────────────────────────────│          │  │  │ │
            │ + SetAnimationAsset()             │          │  │  │ │
            │ + Play(bLooping)                  │          │  │  │ │
            │ + Stop()                           │          │  │  │ │
            │ + Pause()                          │          │  │  │ │
            │ + SetPlayRate()                   │          │  │  │ │
            │ + NativeUpdateAnimation() (override)         │  │  │ │
            └───────────────────────────────────┘          │  │  │ │
                                                           │  │  │ │
┌──────────────────────────────────────────────────────────┼──┼──┼─┼──┐
│                  COMPONENT INTEGRATION (컴포넌트 통합)   │  │  │ │  │
└──────────────────────────────────────────────────────────┼──┼──┼─┼──┘
                                                           │  │  │ │
            ┌────────────────────────────┐                 │  │  │ │
            │  USkinnedMeshComponent     │  (부모)         │  │  │ │
            │────────────────────────────│                 │  │  │ │
            │ + SkeletalMesh: USkeletalMesh*               │  │  │ │
            │ + SetSkeletalMesh()        │                 │  │  │ │
            └────────────┬───────────────┘                 │  │  │ │
                         │                                  │  │  │ │
                         │ ◁───── 상속                     │  │  │ │
                         │                                  │  │  │ │
            ┌────────────▼───────────────┐                 │  │  │ │
            │ USkeletalMeshComponent     │                 │  │  │ │
            │────────────────────────────│                 │  │  │ │
            │ + AnimationMode: EAnimationMode ◀────────────┼──┼──┼─┼──┐
            │ + AnimInstance: UAnimInstance*  ────────────>│  │  │ │  │
            │ + AnimationData: UAnimSequence* ────────────>│  │  │ │  │
            │────────────────────────────│                 │  │  │ │  │
            │ + PlayAnimation()   (Lua)  │                 │  │  │ │  │
            │ + StopAnimation()   (Lua)  │                 │  │  │ │  │
            │ + SetAnimationMode()       │                 │  │  │ │  │
            │ + SetAnimation()            │                 │  │  │ │  │
            │ + Play()                    │                 │  │  │ │  │
            │ + HandleAnimNotify()       │                 │  │  │ │  │
            │ # TickAnimation()           │                 │  │  │ │  │
            │────────────────────────────│                 │  │  │ │  │
            │ + CurrentLocalSpacePose    │                 │  │  │ │  │
            │ + CurrentComponentSpacePose│                 │  │  │ │  │
            └────────────────────────────┘                 │  │  │ │  │
                                                           │  │  │ │  │
┌──────────────────────────────────────────────────────────┼──┼──┼─┼──┼──┐
│                     DATA STRUCTURES (데이터 구조)        │  │  │ │  │  │
└──────────────────────────────────────────────────────────┼──┼──┼─┼──┼──┘
                                                           │  │  │ │  │
┌───────────────────────────┐   ┌──────────────────────┐  │  │  │ │  │
│      FFrameRate           │   │ FAnimNotifyEvent     │◀─┘  │  │ │  │
│───────────────────────────│   │──────────────────────│     │  │ │  │
│ + Numerator: int32        │   │ + TriggerTime: float │     │  │ │  │
│ + Denominator: int32      │   │ + Duration: float    │     │  │ │  │
│───────────────────────────│   │ + NotifyName: FName  │     │  │ │  │
│ + AsDecimal(): float      │   │ + NotifyData: FString│     │  │ │  │
│ + AsFrameNumber()         │   └──────────────────────┘     │  │ │  │
│ + AsSeconds()             │                                 │  │ │  │
└────────────┬──────────────┘                                 │  │ │  │
             │                                                 │  │ │  │
             └─────────────────────────────────────────────────┘  │ │  │
                                                                  │ │  │
┌─────────────────────────────────┐                              │ │  │
│   FRawAnimSequenceTrack         │                              │ │  │
│─────────────────────────────────│                              │ │  │
│ + PosKeys: TArray<FVector>      │                              │ │  │
│ + RotKeys: TArray<FQuat>        │                              │ │  │
│ + ScaleKeys: TArray<FVector>    │                              │ │  │
│─────────────────────────────────│                              │ │  │
│ + IsEmpty(): bool               │                              │ │  │
│ + GetNumKeys(): int32           │                              │ │  │
└────────────┬────────────────────┘                              │ │  │
             │                                                    │ │  │
             │ 포함 (composition) ◆                              │ │  │
             │                                                    │ │  │
┌────────────▼────────────────────┐                              │ │  │
│   FBoneAnimationTrack           │ ◀────────────────────────────┘ │  │
│─────────────────────────────────│                                │  │
│ + Name: FName                   │                                │  │
│ + BoneTreeIndex: int32          │                                │  │
│ + InternalTrack: FRawAnimSequenceTrack                           │  │
└─────────────────────────────────┘                                │  │
                                                                   │  │
┌─────────────────────────────────┐                                │  │
│     FAnimExtractContext         │                                │  │
│─────────────────────────────────│                                │  │
│ + CurrentTime: float            │                                │  │
│ + bExtractRootMotion: bool      │                                │  │
│ + bLooping: bool                │                                │  │
└─────────────────────────────────┘                                │  │
                                                                   │  │
┌─────────────────────────────────┐                                │  │
│        FPoseContext             │                                │  │
│─────────────────────────────────│                                │  │
│ + BoneTransforms: TArray<FTransform>                             │  │
│─────────────────────────────────│                                │  │
│ + SetNumBones()                 │                                │  │
│ + GetNumBones(): int32          │                                │  │
└─────────────────────────────────┘                                │  │
                                                                   │  │
┌─────────────────────────────────┐                                │  │
│      EAnimationMode (enum)      │ ◀──────────────────────────────┘  │
│─────────────────────────────────│                                   │
│ • AnimationSingleNode           │                                   │
│ • AnimationLuaScript            │                                   │
└─────────────────────────────────┘                                   │
                                                                      │
┌──────────────────────────────────────────────────────────────┐     │
│               UTILITY CLASS (유틸리티)                        │     │
└──────────────────────────────────────────────────────────────┘     │
                                                                      │
            ┌─────────────────────────────────┐                      │
            │  «static» FAnimationRuntime     │                      │
            │─────────────────────────────────│                      │
            │ (정적 유틸리티 클래스)           │                      │
            │─────────────────────────────────│                      │
            │ + BlendTwoPosesTogether()       │                      │
            │   (PoseA, PoseB, Alpha, OutPose)│                      │
            │                                 │                      │
            │ + BlendTransforms()             │                      │
            │   (A, B, Alpha): FTransform     │                      │
            └─────────────────────────────────┘                      │
                                                                      │
┌──────────────────────────────────────────────────────────────┐     │
│                    EXTERNAL DEPENDENCIES                      │     │
└──────────────────────────────────────────────────────────────┘     │
                                                                      │
            ┌─────────────────────────────────┐                      │
            │        USkeleton                │ ◀────────────────────┘
            │─────────────────────────────────│
            │ + Bones: TArray<FBone>          │
            │ + BoneNameToIndex: TMap<>       │
            │─────────────────────────────────│
            │ + FindBoneIndex()               │
            │ + GetBoneName()                 │
            └─────────────────────────────────┘


╔══════════════════════════════════════════════════════════════════════════════╗
║                              RELATIONSHIP LEGEND                             ║
╠══════════════════════════════════════════════════════════════════════════════╣
║  ◁───── : 상속 (Inheritance)                                                 ║
║  ────> : 참조/연관 (Association)                                             ║
║  ◆──── : 집합/소유 (Aggregation/Composition)                                 ║
║  UClass : UCLASS (Reflection 지원)                                           ║
║  FStruct : Plain struct (Reflection 없음)                                    ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## 간소화된 상속 계층도

```
┌─────────────────────────────────────────────────────────────┐
│                    INHERITANCE HIERARCHY                    │
└─────────────────────────────────────────────────────────────┘

에셋 계층 (Asset Hierarchy)
────────────────────────────

UResourceBase
    │
    └─ UAnimationAsset (추상)
           │
           └─ UAnimSequenceBase (추상)
                  │
                  └─ UAnimSequence (구체)
                         │
                         ├─ UAnimMontage (미래 확장)
                         └─ UBlendSpace (미래 확장)


런타임 계층 (Runtime Hierarchy)
────────────────────────────────

UObject
    │
    └─ UAnimInstance (추상)
           │
           ├─ UAnimSingleNodeInstance (구체)
           └─ UCustomAnimInstance (사용자 정의)


컴포넌트 계층 (Component Hierarchy)
────────────────────────────────────

UActorComponent
    │
    └─ USceneComponent
           │
           └─ UPrimitiveComponent
                  │
                  └─ UMeshComponent
                         │
                         └─ USkinnedMeshComponent
                                │
                                └─ USkeletalMeshComponent
```

---

## 관계 다이어그램

### 에셋과 런타임의 분리

```
┌───────────────────────────┐
│      ASSET LAYER          │  (디스크에 저장, 불변)
│   (에셋 레이어)            │
└───────────────────────────┘
           │
           │ UAnimSequence
           │ - FrameRate
           │ - BoneAnimationTracks[]
           │
           ▼
┌───────────────────────────┐
│     RUNTIME LAYER         │  (메모리에만 존재, 가변)
│   (런타임 레이어)          │
└───────────────────────────┘
           │
           │ UAnimInstance
           │ - CurrentTime
           │ - bIsPlaying
           │
           ▼
┌───────────────────────────┐
│   COMPONENT LAYER         │  (게임 오브젝트에 붙음)
│   (컴포넌트 레이어)        │
└───────────────────────────┘
           │
           │ USkeletalMeshComponent
           │ - AnimInstance
           │ - AnimationData
           │
           ▼
        [Rendering]
```

### 데이터 흐름

```
┌─────────────────────────────────────────────────────────┐
│                    DATA FLOW                            │
└─────────────────────────────────────────────────────────┘

[TickComponent]
      │
      ▼
[TickAnimation]
      │
      ├─> [UAnimInstance::NativeUpdateAnimation]
      │         │
      │         └─> CurrentTime += DeltaTime
      │
      ├─> [UAnimSequence::GetAnimationPose]
      │         │
      │         └─> [GetBoneTransformAtTime] (각 본마다)
      │                   │
      │                   ├─> InterpolatePosition()
      │                   ├─> InterpolateRotation() (Slerp)
      │                   └─> InterpolateScale()
      │
      ├─> [FPoseContext] (결과 포즈)
      │
      └─> [UpdateFinalSkinningMatrices]
                │
                ▼
          [CPU Skinning]
                │
                ▼
            [Rendering]
```

### 블렌딩 흐름

```
┌─────────────────────────────────────────────────────────┐
│                  BLENDING FLOW                          │
└─────────────────────────────────────────────────────────┘

[Animation A]              [Animation B]
      │                          │
      │ GetAnimationPose()       │
      ▼                          ▼
[FPoseContext A]          [FPoseContext B]
      │                          │
      └──────────┬───────────────┘
                 │
                 │ BlendAlpha = 0.5
                 ▼
[FAnimationRuntime::BlendTwoPosesTogether]
                 │
                 │ for each bone:
                 │   BlendTransforms(A, B, Alpha)
                 ▼
         [FPoseContext Blended]
                 │
                 ▼
         [ApplyToSkeleton]
```

---

## 주요 인터페이스 포인트

### 팀원2가 사용할 인터페이스

```
┌────────────────────────────────────────────────────────┐
│         INTERFACES FOR CUSTOM IMPLEMENTATION          │
└────────────────────────────────────────────────────────┘

UAnimInstance (상속 필요)
    ↓
virtual void NativeUpdateAnimation(float DeltaSeconds)
    - 커스텀 애니메이션 로직 구현
    - State Machine 업데이트
    - Blend Tree 계산

────────────────────────────────────────────────────────

FAnimationRuntime (정적 메서드 사용)
    ↓
BlendTwoPosesTogether(PoseA, PoseB, Alpha, OutPose)
    - 두 포즈 블렌딩
    - State Machine 전환
    - Blend Space 계산

────────────────────────────────────────────────────────

UAnimSequence (데이터 접근)
    ↓
GetAnimationPose(OutPose, Context)
    - 특정 시간의 포즈 추출
    - State에서 사용할 포즈 가져오기

────────────────────────────────────────────────────────

FPoseContext (데이터 전달)
    ↓
BoneTransforms[]
    - State Machine 간 포즈 전달
    - 블렌딩 입출력 데이터
```

---

## 확장 가능한 구조

```
┌────────────────────────────────────────────────────────┐
│              EXTENSIBILITY POINTS                      │
└────────────────────────────────────────────────────────┘

현재 구현:
    UAnimSequence           (키프레임 애니메이션)
    UAnimSingleNodeInstance (단일 애니메이션 재생)

────────────────────────────────────────────────────────

미래 확장:
    UAnimMontage            (섹션 기반 애니메이션)
        └─ 상속: UAnimSequenceBase

    UBlendSpace             (2D 파라미터 블렌딩)
        └─ 상속: UAnimSequenceBase

    UAnimStateMachine       (State Machine)
        └─ 상속: UAnimInstance

    UAnimBlendTree          (Blend Tree)
        └─ 상속: UAnimInstance

────────────────────────────────────────────────────────

확장 방법:
    1. UAnimSequenceBase 상속 → 새로운 에셋 타입
    2. UAnimInstance 상속 → 새로운 재생 로직
    3. 두 가지 모두 조합 가능
```

---

## 메모리 레이아웃

```
┌────────────────────────────────────────────────────────┐
│                  MEMORY LAYOUT                         │
└────────────────────────────────────────────────────────┘

UAnimSequence (에셋, 여러 인스턴스 공유)
├─ FrameRate: 8 bytes
├─ NumberOfFrames: 4 bytes
├─ NumberOfKeys: 4 bytes
├─ SequenceLength: 4 bytes
└─ BoneAnimationTracks: TArray
     └─ FBoneAnimationTrack (본 개수만큼)
          ├─ Name: FName (8 bytes)
          ├─ BoneTreeIndex: 4 bytes
          └─ InternalTrack: FRawAnimSequenceTrack
               ├─ PosKeys: TArray<FVector> (프레임수 * 12 bytes)
               ├─ RotKeys: TArray<FQuat> (프레임수 * 16 bytes)
               └─ ScaleKeys: TArray<FVector> (프레임수 * 12 bytes)

────────────────────────────────────────────────────────

예시: 50개 본, 90프레임 (3초 @ 30fps)
    50 * (8 + 4 + (90*12 + 90*16 + 90*12))
    = 50 * (12 + 90*40)
    = 50 * 3612
    = 180,600 bytes ≈ 176 KB

────────────────────────────────────────────────────────

UAnimSingleNodeInstance (런타임, 캐릭터마다 별도)
├─ CurrentTime: 4 bytes
├─ PreviousTime: 4 bytes
├─ CurrentSequence: 8 bytes (포인터)
├─ bIsPlaying: 1 byte
├─ bLooping: 1 byte
├─ PlayRate: 4 bytes
└─ OwnerComponent: 8 bytes (포인터)

Total: ~30 bytes per instance
```

---

## 파일 구조

```
Source/Runtime/Engine/Animation/
│
├── AnimationTypes.h           # 데이터 구조 정의
│   ├─ FFrameRate
│   ├─ FRawAnimSequenceTrack
│   ├─ FBoneAnimationTrack
│   ├─ FAnimNotifyEvent
│   ├─ FAnimExtractContext
│   ├─ FPoseContext
│   └─ EAnimationMode
│
├── AnimationAsset.h/.cpp      # 에셋 베이스
│   └─ UAnimationAsset
│
├── AnimSequenceBase.h/.cpp    # 재생 가능 에셋 베이스
│   └─ UAnimSequenceBase
│
├── AnimSequence.h/.cpp        # 키프레임 애니메이션
│   └─ UAnimSequence
│
├── AnimInstance.h/.cpp        # 인스턴스 베이스
│   └─ UAnimInstance
│
├── AnimSingleNodeInstance.h/.cpp  # 단일 재생기
│   └─ UAnimSingleNodeInstance
│
└── AnimationRuntime.h/.cpp    # 블렌딩 유틸리티
    └─ FAnimationRuntime

Generated/
│
├── UAnimationAsset.generated.h/.cpp
├── UAnimSequenceBase.generated.h/.cpp
├── UAnimSequence.generated.h/.cpp
├── UAnimInstance.generated.h/.cpp
└── UAnimSingleNodeInstance.generated.h/.cpp
```

---

**문서 작성자**: Claude Code
**최종 업데이트**: 2025-11-14
