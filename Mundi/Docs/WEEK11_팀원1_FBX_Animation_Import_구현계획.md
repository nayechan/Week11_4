# Mundi Engine FBX Animation Import 구현 계획서

**작성일**: 2025-11-14
**담당**: 팀원1 (Animation Core & FBX Import)
**목표**: FBX 파일에서 애니메이션 데이터를 로드하여 UAnimSequence에 저장

---

## 📊 현황 분석

### ✅ 이미 완성된 시스템 (90%)

#### Animation 클래스 계층 (완성)
```
UResourceBase
└── UAnimationAsset (추상)
    └── UAnimSequenceBase (추상)
        └── UAnimSequence (구체)

UObject
└── UAnimInstance (추상)
    └── UAnimSingleNodeInstance (구체)
```

#### 데이터 구조 (완성)
```cpp
// AnimationTypes.h - 모두 구현 완료
struct FRawAnimSequenceTrack {
    TArray<FVector> PosKeys;    // 위치 키프레임
    TArray<FQuat> RotKeys;      // 회전 키프레임 (Quaternion)
    TArray<FVector> ScaleKeys;  // 스케일 키프레임
};

struct FBoneAnimationTrack {
    FName Name;                 // 본 이름
    int32 BoneTreeIndex;        // 스켈레톤 본 인덱스
    FRawAnimSequenceTrack InternalTrack;
};

struct FFrameRate {
    int32 Numerator = 30;
    int32 Denominator = 1;
    float AsDecimal() const;    // 30.0 fps
};
```

#### 핵심 기능 (완성)
- ✅ **보간 시스템**: Linear (Pos/Scale), Slerp (Rotation)
- ✅ **재생 로직**: Play/Stop/Pause/SetPlayRate
- ✅ **루핑**: 시간 래핑 자동 처리
- ✅ **포즈 추출**: `GetAnimationPose()` 완전 구현
- ✅ **블렌딩**: `FAnimationRuntime::BlendTwoPosesTogether()`
- ✅ **Notify 쿼리**: `GetAnimNotifiesInRange()` 구현
- ✅ **리플렉션**: 모든 클래스 `.generated.h/.cpp` 존재

### ❌ 미완성 시스템 (10%)

| 시스템 | 상태 | 우선순위 |
|--------|------|----------|
| **FBX Animation Loading** | 0% | **CRITICAL** |
| **Serialization** | 0% (TODO 주석만 존재) | HIGH |
| **AnimNotify Triggering** | 0% (스텁만 존재) | MEDIUM |

---

## 🎯 구현 목표

### Phase 1: FBX Animation Loading (CRITICAL - Day 1-4)
FBX 파일에서 AnimStack을 로드하여 UAnimSequence의 BoneAnimationTracks에 키프레임 데이터 저장

### Phase 2: Serialization (HIGH - Day 5)
UAnimSequence를 JSON 형식으로 저장/로드하여 씬 파일에 포함 가능

### Phase 3: AnimNotify Triggering (MEDIUM - Day 6-7, 선택적)
애니메이션 재생 중 Notify 이벤트 트리거 (게임플레이 연동)

---

## 📚 UE5 레퍼런스 핵심 요약

### UE5의 FBX Animation Import 방식

#### 1. AnimStack 구조
```
FbxAnimStack (Take/Animation Clip)
  └─ FbxAnimLayer (보통 1개)
      └─ FbxAnimCurveNode (본별, 프로퍼티별)
          └─ FbxAnimCurve (채널별: X, Y, Z)
```

#### 2. 키프레임 추출 API
```cpp
// UE5가 사용하는 FBX SDK API
FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);

// Translation 커브 (X, Y, Z)
FbxAnimCurve* TxCurve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
FbxAnimCurve* TyCurve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
FbxAnimCurve* TzCurve = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

// 키프레임 추출
int32 KeyCount = TxCurve->KeyGetCount();
for (int32 i = 0; i < KeyCount; ++i) {
    FbxTime Time = TxCurve->KeyGetTime(i);
    float TimeSeconds = Time.GetSecondDouble();
    float Value = TxCurve->KeyGetValue(i);
}
```

#### 3. 시간 범위 계산
```cpp
FbxTimeSpan TimeSpan;
AnimStack->GetLocalTimeSpan(TimeSpan);
FbxTime StartTime = TimeSpan.GetStart();
FbxTime EndTime = TimeSpan.GetStop();
float Duration = EndTime.GetSecondDouble() - StartTime.GetSecondDouble();
```

#### 4. 좌표계 변환 (UE5)
UE5는 수동으로 Y 축 반전:
```cpp
// UE5의 ConvertPos
FVector ConvertPosition(FbxVector4 FbxPos) {
    return FVector(FbxPos[0], -FbxPos[1], FbxPos[2]);  // Y 반전
}

// UE5의 ConvertRot
FQuat ConvertRotation(FbxQuaternion FbxQuat) {
    return FQuat(FbxQuat[0], -FbxQuat[1], FbxQuat[2], -FbxQuat[3]);  // Y, W 반전
}
```

#### 5. Euler → Quaternion 변환
```cpp
// FBX SDK 유틸리티 사용
FbxVector4 EulerAngles(RotX, RotY, RotZ);
FbxQuaternion FbxQuat;
FbxQuat.ComposeSphericalXYZ(EulerAngles);
```

---

## 🏗️ Mundi 기존 구조 분석

### 1. UFbxLoader 현황

**파일**: `Mundi/Source/Editor/FBXLoader.h` (46 lines)

#### 이미 구현된 것
```cpp
class UFbxLoader : public UObject {
public:
    static UFbxLoader& GetInstance();  // 싱글톤

    // 스켈레탈 메쉬 로딩 (완성)
    USkeletalMesh* LoadFbxMesh(const FString& FilePath);
    FSkeletalMeshData* LoadFbxMeshAsset(const FString& FilePath);

private:
    // 스켈레톤 로딩 (완성)
    void LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData,
                              int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex);

    // FBX Manager
    FbxManager* SdkManager = nullptr;  // FBX SDK 2020.3.7
};
```

#### FBX Scene 로딩 패턴 (재사용 가능)
```cpp
// LoadFbxMeshAsset() 메서드의 패턴
FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
Importer->Initialize(FilePath.c_str(), -1, SdkManager->GetIOSettings());

FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");
Importer->Import(Scene);
Importer->Destroy();

// 좌표계 변환 (Z-Up, Left-Handed)
FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis,
                               FbxAxisSystem::eParityEven,
                               FbxAxisSystem::eLeftHanded);
UnrealImportAxis.DeepConvertScene(Scene);  // ⚠️ 애니메이션 커브도 자동 변환!

// 단위 변환 (미터)
FbxSystemUnit::m.ConvertScene(Scene);
```

### 2. FSkeleton과 본 매칭

**파일**: `Mundi/Source/Runtime/Core/Misc/VertexData.h`

```cpp
struct FSkeleton {
    FString Name;
    TArray<FBone> Bones;
    TMap<FString, int32> BoneNameToIndex;  // ⚠️ 본 매칭에 사용!
};

struct FBone {
    FString Name;
    int32 ParentIndex;       // -1 = root
    FMatrix BindPose;
    FMatrix InverseBindPose;
};
```

**본 매칭 방법**:
```cpp
// FBX 본 이름으로 스켈레톤 인덱스 찾기
FString BoneName = FString(FbxNode->GetName());
const int32* BoneIndexPtr = TargetSkeleton->BoneNameToIndex.Find(BoneName);
if (!BoneIndexPtr) {
    UE_LOG("Warning: Bone '%s' not found in skeleton, skipping", *BoneName);
    return;  // 스켈레톤에 없는 본은 스킵
}
int32 BoneIndex = *BoneIndexPtr;
```

### 3. UAnimSequence 인터페이스

**파일**: `Mundi/Source/Runtime/Engine/Animation/AnimSequence.h`

```cpp
class UAnimSequence : public UAnimSequenceBase {
public:
    // 이미 구현된 메서드
    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
    void AddBoneTrack(const FBoneAnimationTrack& Track);
    void SetBoneTracks(const TArray<FBoneAnimationTrack>& Tracks);

    // FBX Loader가 접근 가능
    friend class UFbxLoader;  // ⚠️ 이미 준비됨!

    // Properties
    UPROPERTY(EditAnywhere, Category="[애니메이션]")
    FFrameRate FrameRate;  // 기본값 30/1

    UPROPERTY(EditAnywhere, Category="[애니메이션]")
    int32 NumberOfFrames = 0;

    UPROPERTY(EditAnywhere, Category="[애니메이션]")
    int32 NumberOfKeys = 0;

private:
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
};
```

---

## 🛠️ Phase 1: FBX Animation Loading 구현

### 작업 파일
- **수정**: `Mundi/Source/Editor/FBXLoader.h`
- **수정**: `Mundi/Source/Editor/FbxLoader.cpp`

### 추가할 메서드 선언 (FBXLoader.h)

```cpp
class UFbxLoader : public UObject
{
public:
    // 기존 메서드들...

    // 새로 추가할 메서드
    UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton);

private:
    // 새로 추가할 헬퍼 메서드
    void LoadAnimationFromStack(FbxAnimStack* AnimStack,
                                const FSkeleton* TargetSkeleton,
                                UAnimSequence* OutAnim);

    void ExtractBoneAnimationTracks(FbxNode* RootNode,
                                    FbxAnimLayer* AnimLayer,
                                    const FSkeleton* TargetSkeleton,
                                    UAnimSequence* OutAnim);

    void ExtractBoneCurve(FbxNode* BoneNode,
                         FbxAnimLayer* AnimLayer,
                         const FSkeleton* TargetSkeleton,
                         FBoneAnimationTrack& OutTrack);
};
```

### 구현 1: LoadFbxAnimation() - 메인 진입점

**파일**: `FbxLoader.cpp`

```cpp
UAnimSequence* UFbxLoader::LoadFbxAnimation(const FString& FilePath, const FSkeleton* TargetSkeleton)
{
    // 1. 경로 정규화
    FString NormalizedPath = NormalizePath(FilePath);

    // 2. 캐시 확인 (기존 LoadFbxMesh 패턴)
    for (TObjectIterator<UAnimSequence> It; It; ++It)
    {
        UAnimSequence* AnimSeq = *It;
        if (AnimSeq->GetFilePath() == NormalizedPath)
        {
            UE_LOG("Animation already loaded: %s", *NormalizedPath);
            return AnimSeq;
        }
    }

    // 3. FBX Importer 생성 (기존 패턴)
    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if (!Importer->Initialize(NormalizedPath.c_str(), -1, SdkManager->GetIOSettings()))
    {
        UE_LOG("Failed to initialize FBX importer for animation: %s", *NormalizedPath);
        UE_LOG("Error: %s", Importer->GetStatus().GetErrorString());
        return nullptr;
    }

    // 4. FBX Scene 생성 및 Import
    FbxScene* Scene = FbxScene::Create(SdkManager, "Animation Scene");
    Importer->Import(Scene);
    Importer->Destroy();

    // 5. 좌표계 변환 (기존 패턴 - 애니메이션 커브도 자동 변환!)
    FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis,
                                   FbxAxisSystem::eParityEven,
                                   FbxAxisSystem::eLeftHanded);
    FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();
    if (SourceSetup != UnrealImportAxis)
    {
        UE_LOG("Converting animation coordinate system...");
        UnrealImportAxis.DeepConvertScene(Scene);
    }

    // 6. 단위 변환 (기존 패턴)
    FbxSystemUnit::m.ConvertScene(Scene);

    // 7. AnimStack 확인
    int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    if (AnimStackCount == 0)
    {
        UE_LOG("Error: No animation data found in FBX file: %s", *NormalizedPath);
        Scene->Destroy();
        return nullptr;
    }

    UE_LOG("Found %d animation stack(s) in FBX file", AnimStackCount);

    // 8. UAnimSequence 생성
    UAnimSequence* AnimSeq = NewObject<UAnimSequence>();
    AnimSeq->SetFilePath(NormalizedPath);

    // 9. 첫 번째 AnimStack 로드 (대부분의 FBX는 1개만 있음)
    FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
    UE_LOG("Loading animation: %s", AnimStack->GetName());

    LoadAnimationFromStack(AnimStack, TargetSkeleton, AnimSeq);

    // 10. Scene 정리
    Scene->Destroy();

    // 11. 리소스 매니저에 등록 (선택적)
    // UResourceManager::GetInstance().Add<UAnimSequence>(NormalizedPath, AnimSeq);

    UE_LOG("Animation loaded successfully: %d frames, %d bone tracks",
           AnimSeq->NumberOfFrames,
           AnimSeq->GetBoneAnimationTracks().Num());

    return AnimSeq;
}
```

### 구현 2: LoadAnimationFromStack() - AnimStack 파싱

```cpp
void UFbxLoader::LoadAnimationFromStack(FbxAnimStack* AnimStack,
                                        const FSkeleton* TargetSkeleton,
                                        UAnimSequence* OutAnim)
{
    // 1. AnimLayer 가져오기 (보통 첫 번째)
    int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
    if (LayerCount == 0)
    {
        UE_LOG("Error: AnimStack has no layers");
        return;
    }

    FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
    UE_LOG("Using animation layer: %s", AnimLayer->GetName());

    // 2. 시간 범위 추출
    FbxTimeSpan TimeSpan;
    AnimStack->GetLocalTimeSpan(TimeSpan);
    FbxTime StartTime = TimeSpan.GetStart();
    FbxTime EndTime = TimeSpan.GetStop();

    float Duration = static_cast<float>(EndTime.GetSecondDouble() - StartTime.GetSecondDouble());

    if (Duration <= 0.0f)
    {
        UE_LOG("Error: Invalid animation duration: %f", Duration);
        return;
    }

    UE_LOG("Animation duration: %f seconds", Duration);

    // 3. FrameRate 설정 (30fps 기본, FBX에서 추출 가능)
    FFrameRate FrameRate(30, 1);  // 기본값
    // TODO: FBX TimeMode에서 실제 프레임레이트 추출 가능

    int32 NumFrames = static_cast<int32>(Duration * FrameRate.AsDecimal()) + 1;

    OutAnim->FrameRate = FrameRate;
    OutAnim->NumberOfFrames = NumFrames;
    OutAnim->SequenceLength = Duration;

    UE_LOG("Frame rate: %f fps, Frames: %d", FrameRate.AsDecimal(), NumFrames);

    // 4. RootNode부터 본별 애니메이션 추출
    FbxNode* RootNode = AnimStack->GetScene()->GetRootNode();
    ExtractBoneAnimationTracks(RootNode, AnimLayer, TargetSkeleton, OutAnim);

    // 5. NumberOfKeys 계산
    int32 TotalKeys = 0;
    for (const FBoneAnimationTrack& Track : OutAnim->GetBoneAnimationTracks())
    {
        TotalKeys += Track.InternalTrack.GetNumKeys();
    }
    OutAnim->NumberOfKeys = TotalKeys;
}
```

### 구현 3: ExtractBoneAnimationTracks() - Depth-First 순회

```cpp
void UFbxLoader::ExtractBoneAnimationTracks(FbxNode* InNode,
                                            FbxAnimLayer* AnimLayer,
                                            const FSkeleton* TargetSkeleton,
                                            UAnimSequence* OutAnim)
{
    // 기존 LoadSkeletonFromNode 패턴 재사용

    // 1. 현재 노드가 본(Skeleton)인지 확인
    for (int i = 0; i < InNode->GetNodeAttributeCount(); i++)
    {
        FbxNodeAttribute* Attr = InNode->GetNodeAttributeByIndex(i);
        if (!Attr)
            continue;

        if (Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
        {
            // 본 발견! 애니메이션 커브 추출
            FString BoneName = FString(InNode->GetName());

            // 본 매칭
            const int32* BoneIndexPtr = TargetSkeleton->BoneNameToIndex.Find(BoneName);
            if (!BoneIndexPtr)
            {
                UE_LOG("Warning: Bone '%s' not found in skeleton, skipping animation", *BoneName);
                break;  // 스켈레톤에 없는 본은 스킵
            }

            int32 BoneIndex = *BoneIndexPtr;

            // 본 애니메이션 트랙 생성
            FBoneAnimationTrack Track;
            Track.Name = FName(BoneName);
            Track.BoneTreeIndex = BoneIndex;

            // 커브 추출
            ExtractBoneCurve(InNode, AnimLayer, TargetSkeleton, Track);

            // 키프레임이 있는 경우에만 추가
            if (!Track.InternalTrack.IsEmpty())
            {
                OutAnim->AddBoneTrack(Track);
                UE_LOG("Extracted animation for bone '%s': %d keys",
                       *BoneName, Track.InternalTrack.GetNumKeys());
            }

            break;  // 노드당 1개의 Skeleton 속성만 있음
        }
    }

    // 2. Depth-first 재귀 (자식 노드 순회)
    for (int i = 0; i < InNode->GetChildCount(); i++)
    {
        ExtractBoneAnimationTracks(InNode->GetChild(i), AnimLayer, TargetSkeleton, OutAnim);
    }
}
```

### 구현 4: ExtractBoneCurve() - 키프레임 추출 (핵심!)

```cpp
void UFbxLoader::ExtractBoneCurve(FbxNode* BoneNode,
                                  FbxAnimLayer* AnimLayer,
                                  const FSkeleton* TargetSkeleton,
                                  FBoneAnimationTrack& OutTrack)
{
    // 1. Translation 커브 추출 (X, Y, Z)
    FbxAnimCurve* TransX = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* TransY = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* TransZ = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

    // 2. Rotation 커브 추출 (Euler X, Y, Z)
    FbxAnimCurve* RotX = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* RotY = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* RotZ = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

    // 3. Scale 커브 추출 (X, Y, Z)
    FbxAnimCurve* ScaleX = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* ScaleY = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* ScaleZ = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

    // 4. Position 키프레임 추출
    if (TransX && TransY && TransZ)
    {
        int32 KeyCount = TransX->KeyGetCount();
        OutTrack.InternalTrack.PosKeys.Reserve(KeyCount);

        for (int32 i = 0; i < KeyCount; ++i)
        {
            FbxTime Time = TransX->KeyGetTime(i);
            float TimeSeconds = static_cast<float>(Time.GetSecondDouble());

            // ⚠️ DeepConvertScene()이 이미 좌표계 변환했으므로 그대로 사용
            FVector Position(
                static_cast<float>(TransX->KeyGetValue(i)),
                static_cast<float>(TransY->KeyGetValue(i)),
                static_cast<float>(TransZ->KeyGetValue(i))
            );

            // NaN 체크
            if (!Position.IsFinite())
            {
                UE_LOG("Warning: Invalid position value at time %f, using zero", TimeSeconds);
                Position = FVector::ZeroVector;
            }

            OutTrack.InternalTrack.PosKeys.Add(Position);
        }
    }

    // 5. Rotation 키프레임 추출 (Euler → Quaternion 변환)
    if (RotX && RotY && RotZ)
    {
        int32 KeyCount = RotX->KeyGetCount();
        OutTrack.InternalTrack.RotKeys.Reserve(KeyCount);

        for (int32 i = 0; i < KeyCount; ++i)
        {
            FbxTime Time = RotX->KeyGetTime(i);
            float TimeSeconds = static_cast<float>(Time.GetSecondDouble());

            // Euler angles (degrees)
            double EulerX = RotX->KeyGetValue(i);
            double EulerY = RotY->KeyGetValue(i);
            double EulerZ = RotZ->KeyGetValue(i);

            // FBX SDK를 사용한 Euler → Quaternion 변환
            FbxVector4 EulerAngles(EulerX, EulerY, EulerZ);
            FbxQuaternion FbxQuat;
            FbxQuat.ComposeSphericalXYZ(EulerAngles);

            // ⚠️ DeepConvertScene()이 이미 변환했으므로 그대로 사용
            FQuat EngineQuat(
                static_cast<float>(FbxQuat[0]),  // X
                static_cast<float>(FbxQuat[1]),  // Y
                static_cast<float>(FbxQuat[2]),  // Z
                static_cast<float>(FbxQuat[3])   // W
            );

            // Quaternion 정규화
            EngineQuat.Normalize();

            // NaN 체크
            if (!EngineQuat.IsFinite())
            {
                UE_LOG("Warning: Invalid rotation value at time %f, using identity", TimeSeconds);
                EngineQuat = FQuat::Identity;
            }

            OutTrack.InternalTrack.RotKeys.Add(EngineQuat);
        }
    }

    // 6. Scale 키프레임 추출
    if (ScaleX && ScaleY && ScaleZ)
    {
        int32 KeyCount = ScaleX->KeyGetCount();
        OutTrack.InternalTrack.ScaleKeys.Reserve(KeyCount);

        for (int32 i = 0; i < KeyCount; ++i)
        {
            FbxTime Time = ScaleX->KeyGetTime(i);
            float TimeSeconds = static_cast<float>(Time.GetSecondDouble());

            FVector Scale(
                static_cast<float>(ScaleX->KeyGetValue(i)),
                static_cast<float>(ScaleY->KeyGetValue(i)),
                static_cast<float>(ScaleZ->KeyGetValue(i))
            );

            // NaN 체크
            if (!Scale.IsFinite())
            {
                UE_LOG("Warning: Invalid scale value at time %f, using (1,1,1)", TimeSeconds);
                Scale = FVector(1.0f, 1.0f, 1.0f);
            }

            OutTrack.InternalTrack.ScaleKeys.Add(Scale);
        }
    }

    // 7. 키프레임 개수 불일치 처리
    int32 PosCount = OutTrack.InternalTrack.PosKeys.Num();
    int32 RotCount = OutTrack.InternalTrack.RotKeys.Num();
    int32 ScaleCount = OutTrack.InternalTrack.ScaleKeys.Num();

    if (PosCount != RotCount || PosCount != ScaleCount)
    {
        UE_LOG("Warning: Keyframe count mismatch for bone '%s': Pos=%d, Rot=%d, Scale=%d",
               *OutTrack.Name.ToString(), PosCount, RotCount, ScaleCount);
    }
}
```

---

## 🧪 테스트 계획

### Test 1: 단순 회전 애니메이션
**파일**: `TestCube_Rotate.fbx`
**내용**: 큐브 + 1개 본, Z축 360도 회전, 30프레임

**검증**:
```cpp
UAnimSequence* Anim = FbxLoader.LoadFbxAnimation("TestCube_Rotate.fbx", Skeleton);
assert(Anim != nullptr);
assert(Anim->NumberOfFrames == 31);  // 0-30 inclusive
assert(Anim->GetBoneAnimationTracks().Num() == 1);
assert(Anim->GetBoneAnimationTracks()[0].InternalTrack.RotKeys.Num() == 31);

// 첫 프레임과 마지막 프레임의 회전 차이가 360도인지 확인
FQuat FirstRot = Anim->GetBoneAnimationTracks()[0].InternalTrack.RotKeys[0];
FQuat LastRot = Anim->GetBoneAnimationTracks()[0].InternalTrack.RotKeys[30];
// ...
```

### Test 2: 복잡한 캐릭터 애니메이션
**파일**: `Mixamo_Walk.fbx`
**내용**: 50+ 본, 60프레임 걷기 애니메이션

**검증**:
- 모든 본의 키프레임 개수가 동일한지
- Pos/Rot/Scale 개수 일치
- NaN 없음

### Test 3: 계층 구조
**파일**: `ParentChild_Chain.fbx`
**내용**: 부모-자식 3단계 본 체인

**검증**:
- 자식 본의 Transform이 로컬 공간인지 확인
- 부모 본 회전 시 자식 본 영향 받는지

### Test 4: 여러 AnimStack
**파일**: `MultipleAnimations.fbx`
**내용**: 2개 이상의 AnimStack

**검증**:
- 첫 번째 AnimStack만 로드되는지
- 로그에 AnimStack 개수 출력

---

## ✅ 구현 체크리스트

### Phase 1: FBX Animation Loading (Day 1-4)

#### Day 1-2: 기본 구조 (16h)
- [ ] FBXLoader.h에 메서드 선언 추가
  - [ ] LoadFbxAnimation()
  - [ ] LoadAnimationFromStack()
  - [ ] ExtractBoneAnimationTracks()
  - [ ] ExtractBoneCurve()
- [ ] LoadFbxAnimation() 구현
  - [ ] 경로 정규화 및 캐시 확인
  - [ ] FbxImporter 생성 및 Scene 로드
  - [ ] 좌표계/단위 변환
  - [ ] AnimStack 확인
  - [ ] UAnimSequence 생성
- [ ] LoadAnimationFromStack() 구현
  - [ ] AnimLayer 가져오기
  - [ ] FbxTimeSpan 시간 범위 추출
  - [ ] FrameRate, NumberOfFrames 설정
- [ ] 컴파일 확인
- [ ] Test 1: AnimStack 개수 로그 출력 확인

#### Day 3-4: 키프레임 추출 (16h) ⚠️ CRITICAL
- [ ] ExtractBoneAnimationTracks() 구현
  - [ ] Depth-first 순회 (LoadSkeletonFromNode 패턴)
  - [ ] eSkeleton 속성 체크
  - [ ] 본 매칭 (BoneNameToIndex.Find)
  - [ ] ExtractBoneCurve() 호출
- [ ] ExtractBoneCurve() 구현
  - [ ] Translation 커브 추출 (X, Y, Z)
  - [ ] Rotation 커브 추출 (Euler X, Y, Z)
  - [ ] Scale 커브 추출 (X, Y, Z)
  - [ ] Euler → Quaternion 변환 (ComposeSphericalXYZ)
  - [ ] NaN/Infinite 체크
  - [ ] 키프레임 개수 불일치 경고
- [ ] Test 1: 큐브 회전 애니메이션
  - [ ] 키프레임 개수 확인 (31개)
  - [ ] 첫/마지막 회전 값 확인
- [ ] Test 2: Mixamo 캐릭터 걷기
  - [ ] 모든 본 트랙 생성 확인
  - [ ] NaN 없음 확인

### Phase 2: Serialization (Day 5, 8h)
- [ ] UAnimationAsset::Serialize() 구현
  - [ ] Skeleton 경로 저장/로드
- [ ] UAnimSequenceBase::Serialize() 구현
  - [ ] Notifies 배열 직렬화
  - [ ] SequenceLength, RateScale 직렬화
- [ ] UAnimSequence::Serialize() 구현
  - [ ] BoneAnimationTracks 배열 직렬화
  - [ ] FRawAnimSequenceTrack 직렬화
  - [ ] FrameRate, NumberOfFrames 직렬화
- [ ] 저장/로드 테스트
  - [ ] 씬 파일에 애니메이션 포함
  - [ ] 로드 후 키프레임 데이터 일치 확인

### Phase 3: AnimNotify Triggering (Day 6-7, 8h, 선택적)
- [ ] UAnimInstance::TriggerAnimNotifies() 구현
  - [ ] GetAnimNotifiesInRange() 호출
  - [ ] OwnerComponent->HandleAnimNotify() 호출
- [ ] AActor::OnAnimNotify() 가상 메서드 추가 (선택적)
- [ ] 테스트: Notify 트리거 확인

---

## 🚨 주의사항 및 패턴

### 코딩 규약 (MUST FOLLOW)

#### 1. 로깅
```cpp
// ✅ 올바른 방법
UE_LOG("Animation loaded: %d frames", NumFrames);

// ❌ 절대 금지
std::cout << "Animation loaded" << std::endl;
printf("Animation loaded\n");
```

#### 2. 컨테이너
```cpp
// ✅ 올바른 방법
TArray<FBoneAnimationTrack> Tracks;
TMap<FString, int32> BoneMap;

// ❌ 절대 금지
std::vector<FBoneAnimationTrack> Tracks;
std::unordered_map<std::string, int32> BoneMap;
```

#### 3. 본 매칭
```cpp
// ✅ 올바른 방법 - BoneNameToIndex 맵 사용
const int32* BoneIndexPtr = TargetSkeleton->BoneNameToIndex.Find(BoneName);
if (!BoneIndexPtr) {
    UE_LOG("Warning: Bone '%s' not found", *BoneName);
    return;
}
int32 BoneIndex = *BoneIndexPtr;

// ❌ 잘못된 방법 - 선형 탐색
for (int32 i = 0; i < Bones.Num(); ++i) {
    if (Bones[i].Name == BoneName) { ... }
}
```

#### 4. 좌표계 변환
```cpp
// ✅ Mundi: DeepConvertScene() 신뢰
UnrealImportAxis.DeepConvertScene(Scene);
// 이후 모든 Transform은 이미 변환되어 있음

// ❌ UE5 방식 수동 변환 (Mundi에서 불필요)
FVector ConvertedPos(FbxPos[0], -FbxPos[1], FbxPos[2]);  // NO!
```

### 에러 처리 패턴

#### 1. NaN/Infinite 체크
```cpp
if (!Position.IsFinite()) {
    UE_LOG("Warning: Invalid position, using zero");
    Position = FVector::ZeroVector;
}

if (!Rotation.IsFinite()) {
    UE_LOG("Warning: Invalid rotation, using identity");
    Rotation = FQuat::Identity;
}
```

#### 2. 본 미매칭 처리
```cpp
const int32* BoneIndexPtr = TargetSkeleton->BoneNameToIndex.Find(BoneName);
if (!BoneIndexPtr) {
    UE_LOG("Warning: Bone '%s' not found in skeleton, skipping", *BoneName);
    return;  // 스킵 (에러 아님)
}
```

#### 3. AnimStack 없음
```cpp
if (Scene->GetSrcObjectCount<FbxAnimStack>() == 0) {
    UE_LOG("Error: No animation data found");
    return nullptr;  // 에러 반환
}
```

### 메모리 최적화

#### 1. Reserve 사용
```cpp
int32 KeyCount = TransX->KeyGetCount();
OutTrack.InternalTrack.PosKeys.Reserve(KeyCount);
OutTrack.InternalTrack.RotKeys.Reserve(KeyCount);
OutTrack.InternalTrack.ScaleKeys.Reserve(KeyCount);
```

#### 2. 상수 트랙 압축 (선택적)
```cpp
bool IsConstantTrack(const TArray<FVector>& Keys) {
    if (Keys.Num() <= 1) return true;
    for (int32 i = 1; i < Keys.Num(); ++i) {
        if (!Keys[i].Equals(Keys[0], 0.0001f))
            return false;
    }
    return true;
}

if (IsConstantTrack(Track.InternalTrack.PosKeys)) {
    Track.InternalTrack.PosKeys.SetNum(1);  // 1개만 저장
}
```

---

## 📖 참고 자료

### Mundi Engine 내부 참고
- `FBXLoader.cpp` (lines 200-600): 기존 Scene 로딩 패턴
- `FBXLoader.cpp` (lines 400-500): LoadSkeletonFromNode depth-first 패턴
- `VertexData.h` (lines 264-305): FSkeleton, FBone 구조
- `AnimSequence.cpp` (lines 20-100): 보간 알고리즘 참고

### FBX SDK 문서
- FbxAnimStack: Animation take 컨테이너
- FbxAnimLayer: Animation layer
- FbxAnimCurve: 키프레임 커브
- FbxQuaternion::ComposeSphericalXYZ: Euler → Quaternion

### Unreal Engine 소스 코드
- `Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxAnimation.cpp`
  - ImportCurve() 메서드 (lines 59-305)
  - GetFbxTransformCurves() 메서드 (lines 404-466)

---

## 📝 변경 이력

| 날짜 | 버전 | 변경 내용 | 작성자 |
|------|------|----------|--------|
| 2025-11-14 | 1.0 | 초안 작성 | Claude Code |

---

**이 문서는 CLAUDE가 FBX Animation Import를 구현할 때 참고하는 구현 가이드입니다.**
**코드 예시를 최대한 상세히 작성하여 복사-붙여넣기 후 약간만 수정하면 동작하도록 했습니다.**
