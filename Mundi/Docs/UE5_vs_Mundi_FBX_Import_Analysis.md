# UE5 vs Mundi FBX Import Implementation Analysis

**작성일**: 2025-11-15
**목적**: Unreal Engine 5 FBX Import 방식과 Mundi Week11_4 엔진의 FBX Import 방식을 비교 분석하여 정확한 구현을 위한 가이드 제공

---

## Executive Summary

현재 Mundi Week11_4의 FBX Import 구현은 UE5와 **3가지 핵심적인 차이**가 있어 100배 스케일 버그, 비정상적인 본 회전, 애니메이션 재생 시 캐릭터가 눕는 현상 등의 문제가 발생하고 있습니다.

### 발견된 핵심 문제 (Critical Issues)

| # | 문제 | 영향도 | 상태 |
|---|------|--------|------|
| 1 | **ConvertMatrix 구현 오류** | 🔴 CRITICAL | 미수정 |
| 2 | **Vertex 위치 변환 방식 차이** | 🔴 CRITICAL | 미수정 |
| 3 | **ComputeTotalMatrix 로직 누락** | 🔴 CRITICAL | 미수정 |

---

## 1. ConvertMatrix 구현 차이 (CRITICAL)

### 현재 Mundi 구현 (WRONG)

**파일**: `Mundi/Source/Editor/FbxDataConverter.cpp`

```cpp
FMatrix FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(const FbxAMatrix& FbxMatrix)
{
    FMatrix Result;

    // 기본 행렬 복사
    for (int Row = 0; Row < 4; Row++)
    {
        for (int Col = 0; Col < 4; Col++)
        {
            Result.M[Row][Col] = static_cast<float>(FbxMatrix.Get(Row, Col));
        }
    }

    // ❌ WRONG: Row 1 전체를 부호 반전 (M[1][1] 포함)
    Result.M[1][0] = -Result.M[1][0];
    Result.M[1][1] = -Result.M[1][1];  // ⚠️ 이 부분이 잘못됨!
    Result.M[1][2] = -Result.M[1][2];
    Result.M[1][3] = -Result.M[1][3];

    // Column 1 부호 반전 (다른 행들)
    Result.M[0][1] = -Result.M[0][1];
    Result.M[2][1] = -Result.M[2][1];
    Result.M[3][1] = -Result.M[3][1];

    return Result;
}
```

### UE5 구현 (CORRECT)

**파일**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxUtilsImport.cpp:178-202`

```cpp
FMatrix FFbxDataConverter::ConvertMatrix(const FbxAMatrix& Matrix)
{
    FMatrix UEMatrix;

    for(int i=0; i<4; ++i)
    {
        const FbxVector4 Row = Matrix.GetRow(i);

        if(i == 1)  // ✅ Row 1 (Y-axis row) - 특별 처리
        {
            UEMatrix.M[i][0] = -Row[0];  // X 성분 부호 반전
            UEMatrix.M[i][1] = Row[1];   // ✅ Y 성분 유지 (부호 반전 안 함!)
            UEMatrix.M[i][2] = -Row[2];  // Z 성분 부호 반전
            UEMatrix.M[i][3] = -Row[3];  // Translation 부호 반전
        }
        else  // Row 0, 2, 3
        {
            UEMatrix.M[i][0] = Row[0];
            UEMatrix.M[i][1] = -Row[1];  // Y 성분 부호 반전
            UEMatrix.M[i][2] = Row[2];
            UEMatrix.M[i][3] = Row[3];
        }
    }

    return UEMatrix;
}
```

### 차이점 비교표

| 행렬 요소 | Mundi (Wrong) | UE5 (Correct) | 결과 |
|----------|---------------|---------------|------|
| M[0][0] | Row[0] | Row[0] | ✅ Same |
| M[0][1] | -Row[1] | -Row[1] | ✅ Same |
| M[0][2] | Row[2] | Row[2] | ✅ Same |
| M[0][3] | Row[3] | Row[3] | ✅ Same |
| **M[1][0]** | **-Row[0]** | **-Row[0]** | ✅ Same |
| **M[1][1]** | **-Row[1]** ❌ | **Row[1]** ✅ | 🔴 **DIFFERENT!** |
| **M[1][2]** | **-Row[2]** | **-Row[2]** | ✅ Same |
| **M[1][3]** | **-Row[3]** | **-Row[3]** | ✅ Same |
| M[2][0] | Row[0] | Row[0] | ✅ Same |
| M[2][1] | -Row[1] | -Row[1] | ✅ Same |
| M[2][2] | Row[2] | Row[2] | ✅ Same |
| M[2][3] | Row[3] | Row[3] | ✅ Same |
| M[3][0] | Row[0] | Row[0] | ✅ Same |
| M[3][1] | -Row[1] | -Row[1] | ✅ Same |
| M[3][2] | Row[2] | Row[2] | ✅ Same |
| M[3][3] | Row[3] | Row[3] | ✅ Same |

### 왜 M[1][1]을 유지해야 하는가?

**수학적 배경**:
- Y-Flip 변환은 Y축 스케일을 -1로 만드는 것과 같음
- 행렬의 Y-축 스케일은 **Row 1의 길이 (magnitude)**로 표현됨
- Row 1 전체를 부호 반전하면 스케일 방향만 바뀌고 크기는 유지됨
- 하지만 **M[1][1] (Y-Y 성분)**은 Y축이 Y방향으로 얼마나 늘어나는지를 나타냄
- Right-Handed → Left-Handed 변환 시 Y축 방향은 반전되지만, **자기 자신과의 내적(M[1][1])은 양수로 유지**되어야 함

**결론**: M[1][1]을 부호 반전하면 Y축 스케일이 잘못 계산되어 100배 스케일 버그 등이 발생할 수 있음.

---

## 2. Vertex 위치 변환 방식 차이 (CRITICAL)

### 현재 Mundi 구현

**파일**: `Mundi/Source/Editor/FBXLoader.cpp` (LoadMesh 함수 내부)

```cpp
void UFbxLoader::LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, ...)
{
    // ... 중략 ...

    for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);

        // ❌ WRONG: Cluster의 TransformMatrix를 직접 사용
        FbxAMatrix TransformMatrix;
        Cluster->GetTransformMatrix(TransformMatrix);  // ⚠️ 이 방식이 문제!

        // Vertex 위치 변환
        FbxVector4 FinalPosition = TransformMatrix.MultT(ControlPoint);
        FVector ConvertedPosition = FFbxDataConverter::ConvertPos(FinalPosition);
    }
}
```

**문제점**:
- `Cluster->GetTransformMatrix()`는 **메시가 바인딩된 당시의 월드 변환**만 포함
- **GeometricTransform (Pivot, Rotation Offset, Scaling Offset 등)**이 누락됨
- DCC 툴에서 설정한 Geometric 변환이 무시되어 위치/스케일 오류 발생

### UE5 구현 (CORRECT)

**파일**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxSkeletalMeshImport.cpp:1607, 1624-1625`

```cpp
void UnFbx::FFbxImporter::ImportVertices(...)
{
    // ✅ CORRECT: TotalMatrix 계산 (GlobalTransform + GeometricTransform)
    FbxAMatrix TotalMatrix = ComputeSkeletalMeshTotalMatrix(Node, RootNode);

    for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
    {
        FbxVector4 Position = Mesh->GetControlPointAt(VertexIndex);

        // ✅ TotalMatrix로 변환
        FbxVector4 FinalPosition = TotalMatrix.MultT(Position);

        // ✅ 좌표계 변환
        FVector ConvertedPosition = Converter.ConvertPos(FinalPosition);
    }
}
```

### ComputeSkeletalMeshTotalMatrix 구현

**파일**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxMainImport.cpp:2060-2088`

```cpp
FbxAMatrix UnFbx::FFbxImporter::ComputeSkeletalMeshTotalMatrix(
    FbxNode* Node,
    FbxNode* RootNode)
{
    // 1️⃣ GeometricTransform 추출
    FbxVector4 GeometricTranslation = Node->GetGeometricTranslation(FbxNode::eSourcePivot);
    FbxVector4 GeometricRotation = Node->GetGeometricRotation(FbxNode::eSourcePivot);
    FbxVector4 GeometricScaling = Node->GetGeometricScaling(FbxNode::eSourcePivot);

    FbxAMatrix GeometryTransform;
    GeometryTransform.SetT(GeometricTranslation);
    GeometryTransform.SetR(GeometricRotation);
    GeometryTransform.SetS(GeometricScaling);

    // 2️⃣ GlobalTransform 가져오기
    FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(Node);

    // 3️⃣ Pivot Baking (선택적)
    if (bBakePivotInVertex)
    {
        FbxAMatrix PivotGeometry;
        FbxVector4 RotationPivot = Node->GetRotationPivot(FbxNode::eSourcePivot);
        FbxVector4 ScalingPivot = Node->GetScalingPivot(FbxNode::eSourcePivot);
        // ... Pivot 계산 로직 ...
        GeometryTransform = PivotGeometry * GeometryTransform;
    }

    // 4️⃣ TotalMatrix = GlobalTransform * GeometryTransform
    FbxAMatrix TotalMatrix = GlobalTransform * GeometryTransform;

    return TotalMatrix;
}
```

### 비교 요약

| 항목 | Mundi (Wrong) | UE5 (Correct) |
|------|---------------|---------------|
| **변환 행렬 소스** | `Cluster->GetTransformMatrix()` | `ComputeSkeletalMeshTotalMatrix()` |
| **GeometricTransform** | ❌ 누락 | ✅ 포함 |
| **Pivot Baking** | ❌ 누락 | ✅ 포함 (선택적) |
| **공식** | `TransformMatrix * Vertex` | `(GlobalTransform * Geometry) * Vertex` |

---

## 3. ComputeTotalMatrix 로직 누락 (CRITICAL)

### UE5 TotalMatrix 계산 파이프라인

```
┌─────────────────────────────────────────────────────────────┐
│                    FBX Node Hierarchy                       │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 1: Extract Geometric Transform                       │
│  - GeometricTranslation (Pivot Translation)                 │
│  - GeometricRotation (Rotation Offset)                      │
│  - GeometricScaling (Scaling Offset)                        │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 2: Get Global Transform                              │
│  - Scene->GetAnimationEvaluator()->GetNodeGlobalTransform() │
│  - World-space transform at current time                    │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 3: Optional Pivot Baking                             │
│  if (bBakePivotInVertex):                                   │
│    - RotationPivot, ScalingPivot 추출                       │
│    - PivotGeometry 행렬 생성                                │
│    - GeometryTransform = PivotGeometry * GeometryTransform  │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 4: Compute TotalMatrix                               │
│  TotalMatrix = GlobalTransform * GeometryTransform          │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 5: Transform Vertices                                │
│  FinalPosition = TotalMatrix.MultT(ControlPoint)            │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 6: Convert to Engine Coordinate System               │
│  ConvertedPosition = ConvertPos(FinalPosition)              │
└─────────────────────────────────────────────────────────────┘
```

### Mundi에 누락된 항목

1. **GeometricTransform 추출**
   - `Node->GetGeometricTranslation(FbxNode::eSourcePivot)` ❌
   - `Node->GetGeometricRotation(FbxNode::eSourcePivot)` ❌
   - `Node->GetGeometricScaling(FbxNode::eSourcePivot)` ❌

2. **Pivot Baking 로직**
   - `Node->GetRotationPivot(FbxNode::eSourcePivot)` ❌
   - `Node->GetScalingPivot(FbxNode::eSourcePivot)` ❌
   - PivotGeometry 행렬 계산 ❌

3. **TotalMatrix 조합**
   - `GlobalTransform * GeometryTransform` ❌

---

## 4. Animation Import 차이

### UE5 애니메이션 변환 방식

**파일**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxAnimationImport.cpp`

```cpp
void UnFbx::FFbxImporter::ExtractBoneTransform(
    FbxNode* BoneNode,
    FbxAnimLayer* AnimLayer,
    FRawAnimSequenceTrack& Track)
{
    // 1️⃣ TotalMatrix 사용 (Mesh와 동일한 방식)
    FbxAMatrix TotalMatrix = ComputeSkeletalMeshTotalMatrix(BoneNode, RootNode);

    // 2️⃣ 애니메이션 키프레임 평가
    FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()
        ->GetNodeGlobalTransform(BoneNode, KeyTime);

    // 3️⃣ JointPostConversionMatrix 적용 (조건부)
    if (bForceFrontXAxis)
    {
        FbxAMatrix JointPost = GetJointPostConversionMatrix();
        GlobalTransform = GlobalTransform * JointPost;
    }

    // 4️⃣ 좌표계 변환
    FQuat Rotation = Converter.ConvertRotToQuat(GlobalTransform.GetQ());
    FVector Translation = Converter.ConvertPos(GlobalTransform.GetT());
}
```

### Mundi 애니메이션 변환 방식

**파일**: `Mundi/Source/Editor/FBXLoader.cpp` (ExtractBoneCurve 함수)

```cpp
void UFbxLoader::ExtractBoneCurve(...)
{
    // ✅ JointPostConversionMatrix는 올바르게 적용 중
    FbxAMatrix JointPostMatrix = FFbxDataConverter::GetJointPostConversionMatrix(
        CachedScene.bForceFrontXAxis);

    GlobalTransform = GlobalTransform * JointPostMatrix;

    // ✅ 좌표계 변환도 올바름
    FQuat Rotation = FFbxDataConverter::ConvertRotation(GlobalTransform.GetQ());
    FVector Translation = FFbxDataConverter::ConvertPos(GlobalTransform.GetT());
}
```

**평가**: 애니메이션 Import는 **대체로 올바름**. 하지만 ConvertMatrix 버그로 인해 간접적 영향 가능.

---

## 5. 수정 우선순위 및 Action Items

### Phase 1: ConvertMatrix 수정 (HIGHEST PRIORITY)

**파일**: `Mundi/Source/Editor/FbxDataConverter.cpp`

**변경 전**:
```cpp
Result.M[1][0] = -Result.M[1][0];
Result.M[1][1] = -Result.M[1][1];  // ❌ WRONG
Result.M[1][2] = -Result.M[1][2];
Result.M[1][3] = -Result.M[1][3];
```

**변경 후**:
```cpp
Result.M[1][0] = -Result.M[1][0];
Result.M[1][1] = Result.M[1][1];   // ✅ KEEP (부호 반전 제거)
Result.M[1][2] = -Result.M[1][2];
Result.M[1][3] = -Result.M[1][3];
```

### Phase 2: ComputeSkeletalMeshTotalMatrix 구현

**파일**: `Mundi/Source/Editor/FBXLoader.h`

```cpp
class UFbxLoader : public UObject
{
private:
    // 새로 추가할 함수
    FbxAMatrix ComputeSkeletalMeshTotalMatrix(FbxNode* MeshNode, FbxNode* RootNode);
};
```

**파일**: `Mundi/Source/Editor/FBXLoader.cpp`

```cpp
FbxAMatrix UFbxLoader::ComputeSkeletalMeshTotalMatrix(FbxNode* MeshNode, FbxNode* RootNode)
{
    // 1. GeometricTransform 추출
    FbxVector4 Translation = MeshNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    FbxVector4 Rotation = MeshNode->GetGeometricRotation(FbxNode::eSourcePivot);
    FbxVector4 Scaling = MeshNode->GetGeometricScaling(FbxNode::eSourcePivot);

    FbxAMatrix GeometryTransform;
    GeometryTransform.SetT(Translation);
    GeometryTransform.SetR(Rotation);
    GeometryTransform.SetS(Scaling);

    // 2. GlobalTransform 가져오기
    FbxAMatrix GlobalTransform = CachedScene.Scene->GetAnimationEvaluator()
        ->GetNodeGlobalTransform(MeshNode);

    // 3. TotalMatrix 계산
    FbxAMatrix TotalMatrix = GlobalTransform * GeometryTransform;

    return TotalMatrix;
}
```

### Phase 3: LoadMesh 함수 수정

**파일**: `Mundi/Source/Editor/FBXLoader.cpp`

**변경 전**:
```cpp
// ❌ WRONG
FbxAMatrix TransformMatrix;
Cluster->GetTransformMatrix(TransformMatrix);
FbxVector4 FinalPosition = TransformMatrix.MultT(ControlPoint);
```

**변경 후**:
```cpp
// ✅ CORRECT
FbxNode* MeshNode = InMesh->GetNode();
FbxAMatrix TotalMatrix = ComputeSkeletalMeshTotalMatrix(MeshNode, RootNode);
FbxVector4 FinalPosition = TotalMatrix.MultT(ControlPoint);
```

### Phase 4: Pivot Baking 구현 (선택적)

현재는 기본 구현만 하고, 필요 시 추가:

```cpp
// TODO: 필요 시 구현
// FbxVector4 RotationPivot = MeshNode->GetRotationPivot(FbxNode::eSourcePivot);
// FbxVector4 ScalingPivot = MeshNode->GetScalingPivot(FbxNode::eSourcePivot);
```

---

## 6. 테스트 체크리스트

수정 후 다음 항목들을 테스트해야 함:

### 기본 Import 테스트

- [ ] **100배 스케일 버그 해결 확인**
  - FBX Import 후 스케일이 (1, 1, 1)로 정상인지 확인
  - Editor에서 Transform 값 확인

- [ ] **본 회전 상태 확인**
  - Skeleton 뷰에서 모든 본이 정상적으로 표시되는지
  - Bind Pose가 T-Pose 또는 A-Pose로 올바른지

- [ ] **캐릭터 방향 확인**
  - Import된 캐릭터가 서 있는 상태인지 (누워있지 않은지)
  - 전방 방향이 올바른지 (기본: -Y Forward)

### 애니메이션 테스트

- [ ] **애니메이션 재생 시 방향 확인**
  - 애니메이션 재생 시 캐릭터가 눕지 않는지
  - 본 회전이 정상적으로 적용되는지

- [ ] **루트 모션 확인**
  - 이동 애니메이션 재생 시 캐릭터가 올바른 방향으로 이동하는지

### 다양한 FBX 파일 테스트

- [ ] **Maya Export FBX**
  - Maya에서 Export한 FBX가 정상적으로 Import되는지

- [ ] **Blender Export FBX**
  - Blender에서 Export한 FBX가 정상적으로 Import되는지

- [ ] **3ds Max Export FBX**
  - 3ds Max에서 Export한 FBX가 정상적으로 Import되는지

- [ ] **Mixamo Character/Animation**
  - Mixamo에서 다운로드한 FBX가 정상적으로 Import되는지

---

## 7. 예상 결과

위 수정을 완료하면:

1. ✅ **100배 스케일 버그 해결**: ConvertMatrix M[1][1] 수정으로 스케일 계산 정상화
2. ✅ **본 회전 정상화**: TotalMatrix 사용으로 GeometricTransform 반영
3. ✅ **캐릭터 방향 정상화**: -Y Forward 기본값으로 UE5와 동일한 Import 결과
4. ✅ **애니메이션 재생 정상화**: 올바른 좌표계 변환으로 눕는 현상 해결
5. ✅ **일관성 있는 Import**: 모든 FBX 파일이 동일한 방식으로 처리됨

---

## 8. 참고 자료

### UE5 소스 코드 위치

- **좌표 변환 유틸리티**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxUtilsImport.cpp`
  - `ConvertMatrix()`: Lines 178-202

- **Skeletal Mesh Import**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxSkeletalMeshImport.cpp`
  - `ImportVertices()`: Lines 1607, 1624-1625

- **TotalMatrix 계산**: `C:\Dev\UE5\UnrealEngine\Engine\Source\Editor\UnrealEd\Private\Fbx\FbxMainImport.cpp`
  - `ComputeSkeletalMeshTotalMatrix()`: Lines 2060-2088

### Mundi 코드 위치

- **좌표 변환 유틸리티**: `Mundi/Source/Editor/FbxDataConverter.cpp`
- **FBX Loader**: `Mundi/Source/Editor/FBXLoader.cpp`
- **Migration Plan**: `Mundi/Docs/FBX_Import_Migration_Plan.md`

---

## 9. 결론

현재 Mundi Week11_4의 FBX Import는 **3가지 핵심적인 구현 차이**로 인해 UE5와 다른 결과를 생성하고 있습니다:

1. **ConvertMatrix M[1][1] 부호 반전 오류** → 스케일 버그
2. **Cluster Transform 대신 TotalMatrix 사용 필요** → 위치/회전 오류
3. **ComputeTotalMatrix 로직 누락** → GeometricTransform 미반영

위 3가지 항목을 순차적으로 수정하면 UE5와 동일한 FBX Import 결과를 얻을 수 있습니다.

**다음 단계**: Phase 1 (ConvertMatrix 수정)부터 시작하여 순차적으로 구현해야 합니다.
