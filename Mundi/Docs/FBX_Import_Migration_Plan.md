# FBX Import Pipeline Migration Plan
## Week10 방식을 Week11_4에 이식하기

**작성일**: 2025-11-15
**목적**: Week10의 올바른 FBX Import 방식을 Week11_4에 이식하여 100x 스케일 문제와 캐릭터 눕는 문제 해결

---

## 📋 현재 문제점 요약

### 🐛 Bug #1: 100배 스케일 문제
- **증상**: 애니메이션 재생 시 메시가 100배 커짐
- **원인**: `DeepConvertScene()` 사용으로 인한 이중 변환
  - DeepConvertScene이 정점 데이터를 직접 변환
  - GetNodeGlobalTransform()이 변환된 Scene Graph를 다시 읽음
  - 결과: 정점이 두 번 변환됨 (스케일 포함)

### 🐛 Bug #2: 캐릭터 눕는 문제
- **증상**: 애니메이션 재생 시 캐릭터가 서있지 않고 누워있음
- **원인**: Y-Flip 변환 누락
  - Right-Handed → Left-Handed 변환 시 Y축 반전 필요
  - 현재는 Y-Flip 없이 직접 Left-Handed로 변환 시도
  - 정점과 행렬이 서로 다른 좌표계에 존재

### 🐛 Bug #3: Bind Pose 불일치
- **증상**: Bind Pose와 Animation이 다른 좌표 공간 사용
- **원인**:
  - Bind Pose: AnimationEvaluator의 Time 0 (Scene Pose)
  - Animation: AnimationEvaluator의 KeyTime
  - 올바른 Bind Pose: Cluster의 TransformLinkMatrix (Skinning Bind Pose)

---

## 🎯 Week10 vs Week11_4 핵심 차이점

| 구분 | Week10 (정상) | Week11_4 (버그) | 영향 |
|------|---------------|-----------------|------|
| **Scene 변환** | `ConvertScene()` | `DeepConvertScene()` | 이중 변환 → 100x 스케일 |
| **타겟 좌표계** | `eRightHanded` | `eLeftHanded` | Y-Flip 누락 → 누워있음 |
| **변환 순서** | Coordinate → Unit | Unit → Coordinate | 스케일 상호작용 버그 |
| **Bind Pose 소스** | Cluster TransformLinkMatrix | AnimationEvaluator Time 0 | 잘못된 Pose 사용 |
| **Y-Flip 정점** | ✅ ConvertPos() | ❌ 없음 | RH/LH 불일치 |
| **Y-Flip 행렬** | ✅ ConvertFbxMatrixWithYAxisFlip() | ❌ 직접 복사 | 행렬/정점 불일치 |
| **JointPostConversion** | ✅ 스켈레톤+스킨 적용 | ❌ 없음 | 좌표 공간 불일치 |

---

## 🔄 Week10의 3단계 변환 전략

Week10은 **Right-Handed 중간 단계**를 거치는 3단계 변환 사용:

```
FBX 원본 (임의 좌표계)
    ↓
[Stage 1] ConvertScene() → Z-Up, -Y-Forward, RIGHT-HANDED
    ↓ (Scene Graph만 변환, 정점 데이터는 그대로)
[Stage 2] Y-Flip (정점/행렬 추출 시) → Z-Up, X-Forward, LEFT-HANDED
    ↓ (Handedness 변환, Winding Order는 여전히 CCW)
[Stage 3] Index Reversal → CCW를 CW로 변환
    ↓
최종 Mundi 좌표계 (Z-Up, Left-Handed, CW)
```

### Stage 1: ConvertScene (Right-Handed)
```cpp
FbxAxisSystem UnrealImportAxis(
    FbxAxisSystem::eZAxis,           // Up: Z
    FbxAxisSystem::eParityEven,      // Front: -Y (또는 +X)
    FbxAxisSystem::eRightHanded      // ← Right-Handed!
);
UnrealImportAxis.ConvertScene(Scene);  // NOT DeepConvertScene!
```

### Stage 2: Y-Flip (정점/행렬 추출 시)
```cpp
FVector ConvertPos(const FbxVector4& FbxVector)
{
    return FVector(
        static_cast<float>(FbxVector[0]),      // X 그대로
        -static_cast<float>(FbxVector[1]),     // Y 반전 ← 핵심!
        static_cast<float>(FbxVector[2])       // Z 그대로
    );
}
```

### Stage 3: Index Reversal (CCW → CW)
```cpp
// Y-Flip 후에도 Winding Order는 여전히 CCW
// Mundi는 CW = Front Face 사용 (D3D11 기본값)
// 따라서 Index Reversal 필수!
for (int32 i = 0; i < Indices.Num(); i += 3)
{
    std::swap(Indices[i], Indices[i + 2]);  // [v0, v1, v2] → [v2, v1, v0]
}
```

**결과 검증**:
- Coordinate System: Z-Up, X-Forward, Left-Handed ✓
- Winding Order: Clockwise (CW) ✓
- D3D11 호환: FrontCounterClockwise = FALSE (기본값) ✓

---

## 📝 이식 작업 계획 (5단계)

### Phase 1: FFbxDataConverter 유틸리티 클래스 생성 ⭐

**파일**: `Mundi/Source/Editor/FbxDataConverter.h`, `.cpp` (신규 생성)

**목적**: Y-Flip 및 좌표 변환 로직을 중앙 집중화

#### 1.1 FFbxDataConverter.h 생성
```cpp
#pragma once
#include "Source/Runtime/Core/Math/Math.h"
#include "fbxsdk.h"

class FFbxDataConverter
{
public:
    // 정점 위치 변환 (Y-Flip 적용)
    static FVector ConvertPos(const FbxVector4& FbxVector);

    // 법선/탄젠트 변환 (Y-Flip 적용)
    static FVector ConvertDir(const FbxVector4& FbxVector);

    // 회전 변환 (Y-Flip 적용)
    static FQuat ConvertRotation(const FbxQuaternion& FbxQuat);

    // 스케일 변환
    static FVector ConvertScale(const FbxVector4& FbxVector);

    // 행렬 변환 (Y-Flip 적용 - Row 1 전체 + 다른 Row의 Col 1 반전)
    static FMatrix ConvertFbxMatrixWithYAxisFlip(const FbxAMatrix& FbxMatrix);

    // JointPostConversionMatrix 생성 (-Y Forward → +X Forward)
    static FbxAMatrix GetJointPostConversionMatrix(bool bForceFrontXAxis = true);
};
```

#### 1.2 FFbxDataConverter.cpp 구현
```cpp
#include "pch.h"
#include "FbxDataConverter.h"

FVector FFbxDataConverter::ConvertPos(const FbxVector4& FbxVector)
{
    return FVector(
        static_cast<float>(FbxVector[0]),
        -static_cast<float>(FbxVector[1]),  // Y-Flip
        static_cast<float>(FbxVector[2])
    );
}

FVector FFbxDataConverter::ConvertDir(const FbxVector4& FbxVector)
{
    FVector Result = ConvertPos(FbxVector);
    Result.Normalize();
    return Result;
}

FQuat FFbxDataConverter::ConvertRotation(const FbxQuaternion& FbxQuat)
{
    // Quaternion: (X, Y, Z, W)
    // Y-Flip: Negate Y and W components
    return FQuat(
        static_cast<float>(FbxQuat[0]),      // X
        -static_cast<float>(FbxQuat[1]),     // -Y
        static_cast<float>(FbxQuat[2]),      // Z
        -static_cast<float>(FbxQuat[3])      // -W
    );
}

FVector FFbxDataConverter::ConvertScale(const FbxVector4& FbxVector)
{
    // Scale은 Y-Flip 불필요 (양수 스케일 값)
    return FVector(
        static_cast<float>(FbxVector[0]),
        static_cast<float>(FbxVector[1]),
        static_cast<float>(FbxVector[2])
    );
}

FMatrix FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(const FbxAMatrix& FbxMatrix)
{
    FMatrix Result;

    // 1. 기본 복사
    for (int Row = 0; Row < 4; Row++)
    {
        for (int Col = 0; Col < 4; Col++)
        {
            Result.M[Row][Col] = static_cast<float>(FbxMatrix.Get(Row, Col));
        }
    }

    // 2. Y-Axis Row 전체 반전 (Row 1)
    Result.M[1][0] = -Result.M[1][0];
    Result.M[1][1] = -Result.M[1][1];
    Result.M[1][2] = -Result.M[1][2];
    Result.M[1][3] = -Result.M[1][3];  // Translation Y

    // 3. 다른 Row들의 Y Column 반전 (Col 1)
    Result.M[0][1] = -Result.M[0][1];
    Result.M[2][1] = -Result.M[2][1];
    Result.M[3][1] = -Result.M[3][1];

    return Result;
}

FbxAMatrix FFbxDataConverter::GetJointPostConversionMatrix(bool bForceFrontXAxis)
{
    FbxAMatrix JointPostMatrix;
    JointPostMatrix.SetIdentity();

    if (bForceFrontXAxis)
    {
        // -Y Forward → +X Forward 변환
        JointPostMatrix.SetR(FbxVector4(-90.0, -90.0, 0.0));
    }

    return JointPostMatrix;
}
```

**체크리스트**:
- [ ] FbxDataConverter.h 생성
- [ ] FbxDataConverter.cpp 생성
- [ ] 프로젝트에 파일 추가 (Mundi.vcxproj)
- [ ] 빌드 테스트

---

### Phase 2: FBXLoader Scene 변환 로직 수정 ⭐⭐

**파일**: `Mundi/Source/Editor/FBXLoader.cpp`

**함수**: `GetOrLoadFbxScene()`

#### 2.1 ConvertScene 타겟을 Right-Handed로 변경

**현재 코드** (Line 179-185):
```cpp
// WRONG: 직접 Left-Handed로 변환
FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis,
                               FbxAxisSystem::eParityEven,
                               FbxAxisSystem::eLeftHanded);
UnrealImportAxis.DeepConvertScene(Scene);
```

**수정 후**:
```cpp
// CORRECT: Right-Handed 중간 단계로 변환
FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis,
                               FbxAxisSystem::eParityEven,
                               FbxAxisSystem::eRightHanded);  // ← Left → Right
UnrealImportAxis.ConvertScene(Scene);  // ← Deep 제거!
```

#### 2.2 변환 순서 조정 (Unit 변환을 Coordinate 변환 이후로)

**현재 코드** (Line 161-185):
```cpp
// WRONG 순서: Unit → Coordinate
if (ScaleFactor != 100.0)
{
    FbxSystemUnit::m.ConvertScene(Scene);
}
// ...
UnrealImportAxis.DeepConvertScene(Scene);
```

**수정 후**:
```cpp
// CORRECT 순서: Coordinate → Unit
FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis,
                               FbxAxisSystem::eParityEven,
                               FbxAxisSystem::eRightHanded);
UnrealImportAxis.ConvertScene(Scene);  // 1. 좌표계 변환 먼저

Scene->GetAnimationEvaluator()->Reset();  // Evaluator 리셋

// 2. 단위 변환 나중에
if (ScaleFactor != 100.0)
{
    FbxSystemUnit::m.ConvertScene(Scene);
    Scene->GetAnimationEvaluator()->Reset();
}
```

**체크리스트**:
- [ ] Line 179: `eLeftHanded` → `eRightHanded` 변경
- [ ] Line 185: `DeepConvertScene()` → `ConvertScene()` 변경
- [ ] Unit 변환 코드를 Coordinate 변환 이후로 이동
- [ ] 각 ConvertScene 이후 `Scene->GetAnimationEvaluator()->Reset()` 호출 확인

---

### Phase 3: Bind Pose 추출 방식 변경 ⭐⭐⭐

**파일**: `Mundi/Source/Editor/FBXLoader.cpp`

**함수**: `LoadMesh()` - Skinning/Skeleton 처리 부분

#### 3.1 문제점: 현재 Bind Pose 추출 방식

**현재 코드** (Line 638):
```cpp
// WRONG: AnimationEvaluator 사용 (Scene Pose를 가져옴)
FbxAMatrix BoneBindGlobal = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, FBXSDK_TIME_ZERO);
FbxAMatrix BoneBindGlobalInv = BoneBindGlobal.Inverse();
```

**문제점**:
- `GetNodeGlobalTransform(Time 0)`: Scene Pose (노드 기본 자세)
- `Cluster->GetTransformLinkMatrix()`: Skinning Bind Pose (실제 스키닝 바인드 자세)
- **Scene Pose ≠ Skinning Bind Pose** (다를 수 있음!)

#### 3.2 Week10 방식: Cluster 기반 Bind Pose 수집

**새로운 접근 방식**:

##### Step 1: ExtractSkeleton 전에 Cluster에서 Bind Pose 수집
```cpp
// LoadMesh() 함수 내부, ExtractSkeleton 호출 전에 추가

// === Cluster에서 Global Bind Pose 수집 (Week10 Pattern) ===
TMap<FbxNode*, FbxAMatrix> NodeToGlobalBindPoseMap;
FbxAMatrix JointPostMatrix = FFbxDataConverter::GetJointPostConversionMatrix(true);

// 모든 Mesh → Skin → Cluster 순회
for (int32 DeformerIndex = 0; DeformerIndex < Mesh->GetDeformerCount(); DeformerIndex++)
{
    FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin));
    if (!Skin) continue;

    for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        if (!Cluster || !Cluster->GetLink()) continue;

        FbxNode* Joint = Cluster->GetLink();

        // TransformLinkMatrix = Skinning Bind Pose (Global)
        FbxAMatrix GlobalBindPose;
        Cluster->GetTransformLinkMatrix(GlobalBindPose);

        // JointPostConversionMatrix 적용 (-Y Forward → +X Forward)
        GlobalBindPose = GlobalBindPose * JointPostMatrix;

        // Map에 저장
        if (!NodeToGlobalBindPoseMap.Contains(Joint))
        {
            NodeToGlobalBindPoseMap.Add(Joint, GlobalBindPose);
        }
    }
}
```

##### Step 2: ExtractSkeleton에서 Map 사용하여 Local Transform 계산
```cpp
// LoadSkeletonFromNode() 수정 필요

void UFbxLoader::LoadSkeletonFromNode(
    FbxNode* InNode,
    FSkeletalMeshData& MeshData,
    int32 ParentNodeIndex,
    TMap<FbxNode*, int32>& BoneToIndex,
    const TMap<FbxNode*, FbxAMatrix>& NodeToGlobalBindPoseMap)  // ← 추가 파라미터
{
    // ... 기존 코드 ...

    // Bind Pose 계산
    FbxAMatrix ChildGlobalBindPose;
    if (NodeToGlobalBindPoseMap.Contains(InNode))
    {
        ChildGlobalBindPose = NodeToGlobalBindPoseMap[InNode];
    }
    else
    {
        // Map에 없으면 Scene Pose 사용 (Static Mesh용)
        ChildGlobalBindPose = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(InNode, FBXSDK_TIME_ZERO);
        ChildGlobalBindPose = ChildGlobalBindPose * JointPostMatrix;
    }

    // Local Transform 계산
    FbxAMatrix LocalBindPose;
    if (ParentNodeIndex != -1)
    {
        FbxNode* ParentNode = MeshData.Skeleton.Bones[ParentNodeIndex].Node;  // FbxNode 저장 필요
        FbxAMatrix ParentGlobalBindPose = NodeToGlobalBindPoseMap[ParentNode];
        LocalBindPose = ParentGlobalBindPose.Inverse() * ChildGlobalBindPose;
    }
    else
    {
        LocalBindPose = ChildGlobalBindPose;  // Root
    }

    // Y-Flip 적용하여 FMatrix로 변환
    FMatrix BindPoseMatrix = FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(LocalBindPose);

    // Bone에 저장
    MeshData.Skeleton.Bones[BoneIndex].BindPose = BindPoseMatrix;
}
```

##### Step 3: LoadMesh() Skinning 부분에서 GlobalBindPose/InverseBindPose 설정

**현재 코드** (Line 638-648):
```cpp
// WRONG: AnimationEvaluator 사용
FbxAMatrix BoneBindGlobal = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, FBXSDK_TIME_ZERO);
FbxAMatrix BoneBindGlobalInv = BoneBindGlobal.Inverse();

for (int Row = 0; Row < 4; Row++)
    for (int Col = 0; Col < 4; Col++)
    {
        MeshData.Skeleton.Bones[...].BindPose.M[Row][Col] = ...;
        MeshData.Skeleton.Bones[...].InverseBindPose.M[Row][Col] = ...;
    }
```

**수정 후**:
```cpp
// CORRECT: Cluster TransformLinkMatrix 사용
FbxAMatrix TransformMatrix;
Cluster->GetTransformMatrix(TransformMatrix);  // Mesh Global

FbxAMatrix TransformLinkMatrix;
Cluster->GetTransformLinkMatrix(TransformLinkMatrix);  // Bone Global Bind Pose

// JointPostConversionMatrix 적용
FbxAMatrix JointPostMatrix = FFbxDataConverter::GetJointPostConversionMatrix(true);
TransformLinkMatrix = TransformLinkMatrix * JointPostMatrix;

// GlobalBindPoseMatrix 계산 (Y-Flip 적용)
FMatrix GlobalBindPoseMatrix = FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(TransformLinkMatrix);

// InverseBindPoseMatrix 계산 (Y-Flip 적용)
FbxAMatrix InverseBindMatrix = TransformLinkMatrix.Inverse();
FMatrix InverseBindPoseMatrix = FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(InverseBindMatrix);

// 저장
MeshData.Skeleton.Bones[BoneIndex].BindPose = GlobalBindPoseMatrix;
MeshData.Skeleton.Bones[BoneIndex].InverseBindPose = InverseBindPoseMatrix;
```

#### 3.3 데이터 구조 변경 필요성 검토

**FBoneData에 FbxNode 포인터 저장 필요**:
```cpp
// VertexData.h - FBoneData 구조체
struct FBoneData
{
    FString Name;
    int32 ParentIndex;
    FMatrix BindPose;
    FMatrix InverseBindPose;

    // 추가: FBX 노드 참조 (Import 중에만 사용)
    FbxNode* Node = nullptr;  // ← 추가
};
```

**체크리스트**:
- [ ] `LoadMesh()` 시작 부분에 Cluster 기반 Bind Pose 수집 로직 추가
- [ ] `LoadSkeletonFromNode()` 함수 시그니처에 `NodeToGlobalBindPoseMap` 파라미터 추가
- [ ] `LoadSkeletonFromNode()` 내부에서 Map 사용하여 Bind Pose 계산
- [ ] Skinning 부분에서 Cluster TransformLinkMatrix 사용
- [ ] 모든 행렬 변환 시 `ConvertFbxMatrixWithYAxisFlip()` 사용
- [ ] FBoneData에 FbxNode 포인터 추가 (필요 시)

---

### Phase 4: 정점/애니메이션 Y-Flip 및 Index Reversal ⭐⭐

**파일**: `Mundi/Source/Editor/FBXLoader.cpp`

**영향 받는 함수**: `LoadMesh()`, `ExtractBoneCurve()`

**중요**: 이 Phase는 두 가지 핵심 작업을 포함합니다:
1. **Y-Flip**: Right-Handed → Left-Handed 변환
2. **Index Reversal**: CCW → CW Winding Order 변환

#### 4.1 정점 위치 Y-Flip 적용

**현재 코드** (LoadMesh 내부, 정점 추출 부분):
```cpp
// WRONG: 직접 변환 (Y-Flip 없음)
FbxVector4 FbxVertex = ControlPoints[ControlPointIndex];
FVector Position;
Position.X = static_cast<float>(FbxVertex[0]);
Position.Y = static_cast<float>(FbxVertex[1]);  // Y 그대로
Position.Z = static_cast<float>(FbxVertex[2]);
```

**수정 후**:
```cpp
// CORRECT: FFbxDataConverter 사용 (Y-Flip 적용)
FbxVector4 FbxVertex = ControlPoints[ControlPointIndex];
FVector Position = FFbxDataConverter::ConvertPos(FbxVertex);
```

#### 4.2 법선 Y-Flip 적용

**현재 코드**:
```cpp
FbxVector4 FbxNormal;
Mesh->GetPolygonVertexNormal(PolygonIndex, VertexIndex, FbxNormal);
FVector Normal;
Normal.X = static_cast<float>(FbxNormal[0]);
Normal.Y = static_cast<float>(FbxNormal[1]);  // Y 그대로
Normal.Z = static_cast<float>(FbxNormal[2]);
```

**수정 후**:
```cpp
FbxVector4 FbxNormal;
Mesh->GetPolygonVertexNormal(PolygonIndex, VertexIndex, FbxNormal);
FVector Normal = FFbxDataConverter::ConvertDir(FbxNormal);  // Y-Flip + Normalize
```

#### 4.3 탄젠트/바이노말 Y-Flip 적용

**탄젠트**:
```cpp
FbxVector4 FbxTangent;
Mesh->GetPolygonVertexTangent(PolygonIndex, VertexIndex, FbxTangent);
FVector Tangent = FFbxDataConverter::ConvertDir(FbxTangent);
```

**바이노말**:
```cpp
FbxVector4 FbxBinormal;
Mesh->GetPolygonVertexBinormal(PolygonIndex, VertexIndex, FbxBinormal);
FVector Binormal = FFbxDataConverter::ConvertDir(FbxBinormal);
```

#### 4.4 애니메이션 키프레임 Y-Flip 적용

**파일**: `ExtractBoneCurve()` 함수

**현재 코드** (Line 1579-1638):
```cpp
FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, KeyTime);

// Local Transform 계산
FbxVector4 Translation = LocalTransform.GetT();
FbxQuaternion FbxQuat = LocalTransform.GetQ();
FbxVector4 Scaling = LocalTransform.GetS();

// WRONG: 직접 변환 (Y-Flip 없음)
OutTrack.InternalTrack.PosKeys.Add(FVector(
    static_cast<float>(Translation[0]),
    static_cast<float>(Translation[1]),  // Y 그대로
    static_cast<float>(Translation[2])
));

OutTrack.InternalTrack.RotKeys.Add(FQuat(
    static_cast<float>(FbxQuat[0]),
    static_cast<float>(FbxQuat[1]),  // Y 그대로
    static_cast<float>(FbxQuat[2]),
    static_cast<float>(FbxQuat[3])   // W 그대로
));
```

**수정 후**:
```cpp
FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, KeyTime);

// JointPostConversionMatrix 적용
FbxAMatrix JointPostMatrix = FFbxDataConverter::GetJointPostConversionMatrix(true);
GlobalTransform = GlobalTransform * JointPostMatrix;

// Parent Transform도 동일하게 처리
FbxAMatrix ParentGlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(ParentNode, KeyTime);
ParentGlobalTransform = ParentGlobalTransform * JointPostMatrix;

// Local Transform 계산
FbxAMatrix LocalTransform = ParentGlobalTransform.Inverse() * GlobalTransform;

FbxVector4 Translation = LocalTransform.GetT();
FbxQuaternion FbxQuat = LocalTransform.GetQ();
FbxVector4 Scaling = LocalTransform.GetS();

// CORRECT: FFbxDataConverter 사용 (Y-Flip 적용)
OutTrack.InternalTrack.PosKeys.Add(FFbxDataConverter::ConvertPos(Translation));
OutTrack.InternalTrack.RotKeys.Add(FFbxDataConverter::ConvertRotation(FbxQuat));
OutTrack.InternalTrack.ScaleKeys.Add(FFbxDataConverter::ConvertScale(Scaling));
```

#### 4.5 정점 변환 시 Mesh Global Transform 적용

**Week10 방식**: 정점을 Mesh Global 좌표계로 변환

```cpp
// Cluster에서 TransformMatrix 가져오기 (Mesh Global)
FbxAMatrix TransformMatrix;
Cluster->GetTransformMatrix(TransformMatrix);

// 정점을 Mesh Global로 변환
for (uint32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
{
    FbxVector4 LocalVertex = ControlPoints[VertexIndex];
    FbxVector4 GlobalVertex = TransformMatrix.MultT(LocalVertex);  // Transform 적용

    FVector Position = FFbxDataConverter::ConvertPos(GlobalVertex);  // Y-Flip
    // ... 저장
}
```

#### 4.6 Index Reversal (CCW → CW) ⭐⭐⭐ 중요!

**배경**: Y-Flip은 **Handedness만 변경**하고 **Winding Order는 변경하지 않습니다**.

##### 기하학적 원리

많은 개발자들이 Y축 반전이 winding order를 자동으로 반전시킨다고 오해하지만, **실제로는 그렇지 않습니다**.

**예제: X-Z 평면의 삼각형**
```
Original (Right-Handed, CCW):
v0(0, 1, 0) → v1(1, 0, 0) → v2(0, 0, 0)

After Y-Flip:
v0(0, -1, 0) → v1(1, 0, 0) → v2(0, 0, 0)

카메라가 +Z에서 바라볼 때:
- Y좌표만 변경됨 (1 → -1)
- X-Z 평면에서의 순서는 여전히 CCW!
```

##### Mundi vs Unreal Engine

| 항목 | Unreal Engine | Mundi Engine |
|------|---------------|--------------|
| **D3D11 설정** | `FrontCounterClockwise = TRUE` | `FrontCounterClockwise = FALSE` (기본) |
| **Front Face** | CCW | CW |
| **Index Reversal** | ❌ 불필요 | ✅ 필수 |
| **이유** | Y-flip 후 CCW를 그대로 front로 인식 | Y-flip 후 CCW→CW 변환 필요 |

##### 구현 방법

**LoadMesh() 함수 내부, 모든 정점/법선 처리 완료 후**:

```cpp
// Index Reversal (CCW → CW)
// Y-Flip 후에도 삼각형은 CCW로 남아있으므로, Mundi의 CW 기준에 맞추기 위해 반전
for (int32 i = 0; i < Indices.Num(); i += 3)
{
    // 삼각형의 첫번째와 세번째 정점 인덱스 교체
    // [v0, v1, v2] → [v2, v1, v0]
    std::swap(Indices[i], Indices[i + 2]);
}
```

**위치**:
- Static Mesh: `LoadMesh()` 끝, 정점 추출 완료 후
- Skeletal Mesh: `LoadMesh()` 끝, ExtractSkinWeights 완료 후

**주의사항**:
- Index Reversal은 **반드시 Y-Flip 이후**에 수행
- 모든 메시 타입(Static, Skeletal)에 적용 필수
- 삼각형 단위(3개씩)로 처리

##### 검증 방법

D3D11 Rasterizer State로 테스트 가능:

```cpp
// 테스트 1: 모든 면 보이기
rasterizerDesc.CullMode = D3D11_CULL_NONE;
// 결과: 모든 면이 보여야 함

// 테스트 2: CCW를 Front Face로 설정 (Unreal Engine 방식)
rasterizerDesc.FrontCounterClockwise = TRUE;
// 결과: Index Reversal 없이도 올바르게 보여야 함
```

**체크리스트**:
- [ ] 정점 위치: `ConvertPos()` 사용
- [ ] 법선: `ConvertDir()` 사용
- [ ] 탄젠트: `ConvertDir()` 사용
- [ ] 바이노말: `ConvertDir()` 사용
- [ ] 애니메이션 Translation: `ConvertPos()` 사용
- [ ] 애니메이션 Rotation: `ConvertRotation()` 사용
- [ ] 애니메이션 Scale: `ConvertScale()` 사용
- [ ] 애니메이션에 JointPostConversionMatrix 적용
- [ ] 정점에 Mesh Global Transform 적용
- [ ] **Index Reversal (CCW → CW) 적용** ⭐

---

### Phase 5: Root Joint 특수 처리 (선택 사항) ⭐

**배경**: UE5는 Root Joint의 Parent에 JointPostConversionMatrix를 적용하지 않음

**파일**: `ExtractBoneCurve()`

**Root Joint 판별**:
```cpp
bool bIsRootJoint = (BoneNode->GetParent() == nullptr) ||
                    (BoneNode->GetParent()->GetNodeAttribute() == nullptr) ||
                    (BoneNode->GetParent()->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton);
```

**Root Joint 처리**:
```cpp
FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(BoneNode, KeyTime);
GlobalTransform = GlobalTransform * JointPostMatrix;

FbxAMatrix LocalTransform;
if (ParentNode)
{
    FbxAMatrix ParentGlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(ParentNode, KeyTime);

    if (!bIsRootJoint)  // Root Joint가 아니면 Parent에도 JointPost 적용
    {
        ParentGlobalTransform = ParentGlobalTransform * JointPostMatrix;
    }

    LocalTransform = ParentGlobalTransform.Inverse() * GlobalTransform;
}
else
{
    LocalTransform = GlobalTransform;
}
```

**체크리스트**:
- [ ] Root Joint 판별 로직 추가
- [ ] ExtractBoneCurve에서 Root Joint 특수 처리
- [ ] ExtractSkeleton에서도 동일 로직 적용

---

## 🧪 테스트 체크리스트

### ✅ 단계별 테스트

#### Phase 1 완료 후
- [ ] 프로젝트 빌드 성공
- [ ] FFbxDataConverter::ConvertPos() 단위 테스트 (수동)
  - Input: FbxVector4(1, 2, 3, 1)
  - Expected: FVector(1, -2, 3)

#### Phase 2 완료 후
- [ ] 프로젝트 빌드 성공
- [ ] FBX 파일 로드 시 로그 확인:
  - "ConvertScene (Coordinate) applied" 메시지
  - "ConvertScene (Unit) applied" 메시지
  - ScaleFactor 로그 확인 (100.0 = m 단위)

#### Phase 3 완료 후
- [ ] 프로젝트 빌드 성공
- [ ] Skeletal Mesh Import 테스트
  - Bind Pose가 정상적으로 표시되는가?
  - 스케일이 1배인가? (100배 아님)
  - 캐릭터가 서있는가? (누워있지 않음)

#### Phase 4 완료 후
- [ ] 프로젝트 빌드 성공
- [ ] Animation Import 및 재생 테스트
  - 애니메이션 재생 시 스케일 1배 유지?
  - 캐릭터가 서있는 상태로 애니메이션 재생?
  - Bind Pose와 첫 프레임이 일치하는가?
  - 본 회전이 자연스러운가?

#### Phase 5 완료 후 (Root Joint 처리)
- [ ] Root Bone 애니메이션이 정상적으로 작동하는가?

### ✅ 최종 통합 테스트

#### 테스트 시나리오 1: Bind Pose 표시
1. FBX 파일에서 Skeletal Mesh Import
2. Bind Pose 표시
3. **예상 결과**:
   - ✅ 스케일 1배 (100배 아님)
   - ✅ 캐릭터가 서있음 (누워있지 않음)
   - ✅ T-Pose 또는 A-Pose 정상 표시

#### 테스트 시나리오 2: 애니메이션 재생
1. 동일 FBX 파일에서 Animation Import
2. Skeletal Mesh에 애니메이션 적용 및 재생
3. **예상 결과**:
   - ✅ 애니메이션 재생 중 스케일 1배 유지
   - ✅ 캐릭터가 서있는 상태로 애니메이션
   - ✅ 본 회전이 자연스럽고 뒤틀림 없음
   - ✅ 첫 프레임이 Bind Pose와 일치

#### 테스트 시나리오 3: 다양한 FBX 파일
- [ ] Blender 내보내기 FBX (Right-Handed, CCW)
- [ ] Maya 내보내기 FBX (Right-Handed, CCW)
- [ ] 3ds Max 내보내기 FBX (Right-Handed, CCW)
- [ ] cm 단위 FBX
- [ ] m 단위 FBX

#### 회귀 테스트
- [ ] Static Mesh Import 여전히 작동?
- [ ] Material Import 여전히 작동?
- [ ] 기존 씬 파일 로드 가능?

---

## 🔍 디버깅 가이드

### 100배 스케일 문제가 여전히 발생하는 경우

**체크포인트**:
1. `DeepConvertScene()` → `ConvertScene()`으로 변경했는가?
2. 정점 변환 시 `ConvertPos()` 사용했는가?
3. Unit 변환이 Coordinate 변환 이후에 실행되는가?
4. Bind Pose 행렬에 `ConvertFbxMatrixWithYAxisFlip()` 적용했는가?

**디버깅 로그 추가**:
```cpp
UE_LOG("ScaleFactor: %.2f", ScaleFactor);
UE_LOG("ControlPoint[0]: (%.2f, %.2f, %.2f)",
    ControlPoints[0][0], ControlPoints[0][1], ControlPoints[0][2]);
FVector Pos = FFbxDataConverter::ConvertPos(ControlPoints[0]);
UE_LOG("Converted Position: (%.2f, %.2f, %.2f)", Pos.X, Pos.Y, Pos.Z);
```

### 캐릭터 눕는 문제가 여전히 발생하는 경우

**체크포인트**:
1. ConvertScene 타겟이 `eRightHanded`인가?
2. 모든 정점에 `ConvertPos()` (Y-Flip) 적용했는가?
3. 모든 법선/탄젠트에 `ConvertDir()` 적용했는가?
4. 행렬에 `ConvertFbxMatrixWithYAxisFlip()` 적용했는가?
5. 애니메이션 키프레임에 `ConvertPos()`, `ConvertRotation()` 적용했는가?

**좌표계 확인 로그**:
```cpp
FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();
UE_LOG("Scene UpVector: %d", SceneAxisSystem.GetUpVector());
UE_LOG("Scene FrontVector: %d", SceneAxisSystem.GetFrontVector());
UE_LOG("Scene CoordSystem: %d (0=Right, 1=Left)", SceneAxisSystem.GetCoorSystem());
```

**예상 결과** (ConvertScene 이후):
- UpVector: Z (2)
- FrontVector: -Y 또는 +X
- CoordSystem: RightHanded (0)

### Bind Pose와 Animation 불일치하는 경우

**체크포인트**:
1. Bind Pose를 Cluster TransformLinkMatrix에서 가져왔는가? (AnimationEvaluator 아님)
2. Bind Pose와 Animation 모두에 JointPostConversionMatrix 적용했는가?
3. Bind Pose와 Animation 모두에 Y-Flip 적용했는가?

**Bind Pose 매트릭스 로그**:
```cpp
FbxAMatrix GlobalBindPose;
Cluster->GetTransformLinkMatrix(GlobalBindPose);
UE_LOG("Bind Pose Translation: (%.2f, %.2f, %.2f)",
    GlobalBindPose.GetT()[0], GlobalBindPose.GetT()[1], GlobalBindPose.GetT()[2]);

FMatrix Converted = FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(GlobalBindPose);
UE_LOG("Converted Bind Pose Translation: (%.2f, %.2f, %.2f)",
    Converted.M[3][0], Converted.M[3][1], Converted.M[3][2]);
```

---

## 📊 예상 작업 시간

| Phase | 작업 내용 | 예상 시간 | 난이도 |
|-------|----------|-----------|--------|
| Phase 1 | FFbxDataConverter 생성 | 30분 | ⭐ 쉬움 |
| Phase 2 | Scene 변환 로직 수정 | 20분 | ⭐ 쉬움 |
| Phase 3 | Bind Pose 추출 변경 | 1.5시간 | ⭐⭐⭐ 어려움 |
| Phase 4 | Y-Flip + Index Reversal | 1시간 15분 | ⭐⭐ 보통 |
| Phase 5 | Root Joint 처리 | 30분 | ⭐ 쉬움 (선택) |
| **테스트** | 통합 테스트 및 디버깅 | 1-2시간 | ⭐⭐ 보통 |
| **총합** |  | **약 5.5시간** |  |

---

## 📚 참고 자료

### Week10 Mundi 문서
- 경로: `C:\Users\Jungle\source\repos\Mundi_Week10\Mundi\Documentation\Mundi_FBX_Import_Pipeline.md`
- 핵심 섹션:
  - Line 47-66: ConvertScene 설정
  - Line 117-129: ConvertPos (Y-Flip)
  - Line 329: ConvertScene 호출
  - Line 391-426: Cluster 기반 Bind Pose 수집
  - Line 1100-1122: ConvertFbxMatrixWithYAxisFlip

### FBX SDK 문서
- `FbxAxisSystem::ConvertScene()`: Scene Graph만 변환
- `FbxAxisSystem::DeepConvertScene()`: Scene Graph + Geometry 변환
- `FbxCluster::GetTransformLinkMatrix()`: Skinning Bind Pose
- `FbxScene::GetAnimationEvaluator()`: 시간별 Transform 평가

### 좌표계 변환 이론
- Right-Handed → Left-Handed: Y축 반전
- Matrix Y-Flip: Row 1 전체 + 다른 Row의 Col 1 반전
- Quaternion Y-Flip: Y와 W 성분 반전

---

## ✅ 구현 완료 체크리스트

### Phase 1: FFbxDataConverter
- [ ] FbxDataConverter.h 생성
- [ ] FbxDataConverter.cpp 생성
- [ ] ConvertPos() 구현
- [ ] ConvertDir() 구현
- [ ] ConvertRotation() 구현
- [ ] ConvertScale() 구현
- [ ] ConvertFbxMatrixWithYAxisFlip() 구현
- [ ] GetJointPostConversionMatrix() 구현
- [ ] 프로젝트에 파일 추가
- [ ] 빌드 성공

### Phase 2: Scene 변환
- [ ] eLeftHanded → eRightHanded 변경
- [ ] DeepConvertScene() → ConvertScene() 변경
- [ ] Unit 변환을 Coordinate 변환 이후로 이동
- [ ] AnimationEvaluator Reset 호출 추가
- [ ] 빌드 성공

### Phase 3: Bind Pose
- [ ] Cluster 기반 Bind Pose 수집 로직 추가
- [ ] NodeToGlobalBindPoseMap 생성
- [ ] LoadSkeletonFromNode에 Map 파라미터 추가
- [ ] LoadSkeletonFromNode에서 Map 사용
- [ ] JointPostConversionMatrix 적용
- [ ] ConvertFbxMatrixWithYAxisFlip 사용
- [ ] Skinning에서 TransformLinkMatrix 사용
- [ ] FBoneData에 FbxNode 포인터 추가 (필요 시)
- [ ] 빌드 성공

### Phase 4: Y-Flip 및 Index Reversal
- [ ] 정점 위치 ConvertPos() 적용
- [ ] 법선 ConvertDir() 적용
- [ ] 탄젠트 ConvertDir() 적용
- [ ] 바이노말 ConvertDir() 적용
- [ ] 애니메이션 Translation ConvertPos() 적용
- [ ] 애니메이션 Rotation ConvertRotation() 적용
- [ ] 애니메이션 Scale ConvertScale() 적용
- [ ] 애니메이션 JointPostConversionMatrix 적용
- [ ] **Index Reversal (CCW → CW) 적용** ⭐ 필수
- [ ] 빌드 성공

### Phase 5: Root Joint
- [ ] Root Joint 판별 로직 추가
- [ ] ExtractBoneCurve에서 Root Joint 처리
- [ ] ExtractSkeleton에서 Root Joint 처리
- [ ] 빌드 성공

### 최종 테스트
- [ ] Bind Pose 스케일 1배
- [ ] Bind Pose 서있음
- [ ] 애니메이션 스케일 1배
- [ ] 애니메이션 서있음
- [ ] 본 회전 정상
- [ ] 모든 회귀 테스트 통과

---

**작성자**: Claude Code
**버전**: 1.0
**최종 수정**: 2025-11-15
