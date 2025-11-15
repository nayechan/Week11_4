# FBX Import Refactoring Plan - UE5 Architecture Migration

**Created**: 2025-11-15
**Goal**: Refactor Mundi's monolithic `UFbxLoader` class into a modular architecture matching Unreal Engine 5's Interchange plugin patterns
**Strategy**: Incremental refactoring in 7 phases, maintaining functionality after each phase

---

## Executive Summary

### Current Problem
- `UFbxLoader` is a monolithic class (1839 lines) with all FBX import logic mixed together
- Mesh loading, skeleton extraction, animation processing, material parsing, and coordinate conversion all in one class
- Difficult to maintain, test, and extend
- Easy to make mistakes when applying UE5 patterns (e.g., forgotten BindPose matrices in multi-mesh attempt)

### Mundi's Refactoring Approach
- Distributes ~2500 lines across 6+ specialized classes
- Each class has a single, well-defined responsibility
- **Immediate loading** (no payload pattern - simplified for Mundi's needs)
- Template-based coordinate conversion
- Direct implementation (no interface abstraction - easier to maintain)

### Target Architecture

```
Mundi/Source/Editor/FBX/
├── FbxConvert.h/.cpp        # Coordinate conversion utilities (~200 lines)
├── FbxHelper.h/.cpp         # Naming utilities (~200 lines)
├── FbxMaterial.h/.cpp       # Material/texture extraction (~300 lines)
├── FbxAnimation.h/.cpp      # Animation extraction (~400 lines)
├── FbxScene.h/.cpp          # Scene hierarchy and skeleton (~500 lines)
├── FbxMesh.h/.cpp           # Mesh extraction (~800 lines)
└── FbxParser.h/.cpp         # Orchestration and immediate loading (~400 lines)

Mundi/Source/Editor/
└── FBXLoader.h/.cpp         # Thin wrapper with .bin/.anim.bin caching
```

**Architecture Flow**:
```
UFbxLoader (thin wrapper, .bin/.anim.bin caching)
└── FFbxParser (orchestration, immediate loading)
    ├── FFbxConvert (coordinate conversion)
    ├── FFbxHelper (naming utilities)
    ├── FFbxMaterial (material extraction)
    ├── FFbxAnimation (animation extraction)
    ├── FFbxScene (scene/skeleton extraction)
    └── FFbxMesh (mesh extraction)
```

---

## UE5 Architecture Analysis

### UE5 Class Responsibilities

**FFbxConvert** (169 lines)
- Coordinate system conversion (Right-Handed to Left-Handed)
- Template-based for precision support (float/double)
- Position conversion (Y-Flip)
- Rotation conversion (Quaternion with Y/W negation)
- Matrix conversion (M[1][1] preserved - critical!)
- Scene-level axis/unit conversion

**FFbxHelper** (~200 lines)
- Mesh name/ID generation
- Node hierarchy name generation
- FBX namespace handling
- Name clash resolution
- Material name sanitization
- **NOTE**: FPayloadContextBase removed (no payload pattern in Mundi)

**FFbxMaterial** (443 lines)
- Texture extraction and node creation
- Material property parsing
- Material-to-mesh node linking
- Texture path resolution
- Material instance creation

**FFbxAnimation** (~400 lines)
- Skeletal bone animation extraction
- Morph target animation curves
- Rigid body animation
- Custom property animation
- Animation curve baking
- **Immediate extraction**: Returns UAnimSequence* directly

**FFbxScene** (1312 lines)
- Scene hierarchy traversal
- Skeleton structure extraction
- Joint detection and validation
- Bind pose validation
- Geometric transform extraction
- Recursive node processing

**FFbxMesh** (~800 lines)
- Mesh discovery and node creation
- Vertex/triangle extraction
- Skinning data extraction (clusters, weights)
- Static mesh processing
- Skeletal mesh processing
- Morph target extraction
- Bind pose matrix calculation
- Material slot handling

**FFbxParser** (~400 lines)
- FBX SDK initialization and management
- Scene loading with immediate data extraction
- Orchestrates all specialized classes
- **Direct loading API**: LoadFbxMesh(), LoadFbxAnimation()
- Manages FbxScene* caching (SDK-level, not payload)
- Handles cleanup and lifecycle

### Key Architectural Patterns

#### 1. Single Responsibility Principle
Each class has ONE clear job. No mixing of concerns.

#### 2. Immediate Loading Pattern
```cpp
// FFbxParser loads data immediately, no deferred payload system
class FFbxParser
{
public:
    // 직접 메시를 로드하고 반환 (페이로드 시스템 없음)
    USkeletalMesh* LoadFbxMesh(const FString& FilePath);

    // 직접 애니메이션을 로드하고 반환
    UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* Skeleton);

private:
    // FbxScene* 캐싱 (SDK 레벨, 메시와 애니메이션 임포트 간 공유)
    FCachedFbxScene CachedScene;
};
```

**Benefits**:
- Simpler architecture, easier to understand
- Direct method calls, no indirection
- Still allows FbxScene* caching for mesh+animation imports
- Reduces complexity while maintaining functionality

#### 3. Template-Based Conversion
```cpp
template<typename VectorType>
static VectorType ConvertPos(const FbxVector4& FbxVector)
{
    return VectorType(
        static_cast<typename VectorType::FReal>(FbxVector[0]),
        -static_cast<typename VectorType::FReal>(FbxVector[1]),  // Y-Flip
        static_cast<typename VectorType::FReal>(FbxVector[2])
    );
}
```

**Benefits**:
- Supports both float (FVector) and double (FVector3d)
- Type-safe conversion
- No code duplication

---

## Refactoring Phases

### Phase 1: Extract FFbxConvert

**Goal**: Separate coordinate conversion from import logic

**Files to Create**:
- `Source/Editor/FBX/FbxConvert.h`
- `Source/Editor/FBX/FbxConvert.cpp`

**Estimated Size**: ~200 lines

**Tasks**:
1. Create `FFbxConvert` struct (not class) with static methods
2. Move functions from `FbxDataConverter`:
   - `ConvertPos()` - Position conversion (Y-Flip)
   - `ConvertDir()` - Direction conversion (Y-Flip + normalize)
   - `ConvertRotation()` - Quaternion conversion (Y/W negation)
   - `ConvertScale()` - Scale conversion (no Y-Flip)
   - `ConvertFbxMatrixWithYAxisFlip()` - Matrix conversion (preserve M[1][1]!)
3. Make template-based for future precision support
4. Update `FBXLoader.cpp` to use `FFbxConvert::` instead of `FFbxDataConverter::`
5. Add comprehensive comments explaining Y-Flip logic

**Important Note**:
- `JointOrientationMatrix` is NOT a function in FFbxConvert
- It's a **member variable** of `FFbxParser` (FbxAMatrix JointOrientationMatrix)
- It's set during `ConvertScene()` based on bForceFrontXAxis flag
- Access it as: `Parser.JointOrientationMatrix` (not a function call)

**Success Criteria**:
- ✅ Build succeeds
- ✅ Existing FBX imports work identically
- ✅ All conversion logic is in FFbxConvert

**Testing**:
- Import skeletal meshes (verify no 100x scale, correct orientation)
- Import animations (verify bones animate correctly)
- Import multi-material meshes (verify materials apply correctly)

---

### Phase 2: Extract FFbxHelper

**Goal**: Centralize naming and utility functions

**Files to Create**:
- `Source/Editor/FBX/FbxHelper.h`
- `Source/Editor/FBX/FbxHelper.cpp`

**Estimated Size**: ~200 lines

**Tasks**:
1. Create `FFbxHelper` struct (not class) with static methods
2. Move naming functions from `FBXLoader`:
   - Mesh name generation
   - Mesh unique ID generation
   - Node hierarchy name generation
   - Material name sanitization
3. Add utility functions:
   - `GetFbxNodeHierarchyName()` - Full hierarchical path
   - `GetFbxObjectName()` - Sanitize object names
   - `ManageNamespace()` - Handle FBX namespaces
4. Update `FBXLoader.cpp` to use `FFbxHelper::`
5. **IMPORTANT**: Do NOT add FPayloadContextBase
   - Payload context pattern is removed from this refactoring
   - FFbxHelper contains ONLY naming and utility functions

**Success Criteria**:
- ✅ Build succeeds
- ✅ Mesh names unchanged from before
- ✅ Material names handled correctly
- ✅ All naming logic is in FFbxHelper

**Testing**:
- Import FBX with multiple meshes (verify unique names)
- Import FBX with materials (verify material names)
- Import FBX with namespaces (verify namespace handling)

---

### Phase 3: Extract FFbxMaterial

**Goal**: Isolate material and texture parsing

**Files to Create**:
- `Source/Editor/FBX/FbxMaterial.h`
- `Source/Editor/FBX/FbxMaterial.cpp`

**Estimated Size**: ~300 lines

**Tasks**:
1. Create `FFbxMaterial` class
2. Move functions from `FBXLoader`:
   - `ParseMaterial()` → `FFbxMaterial::ExtractMaterialProperties()`
   - `ParseTexturePath()` → `FFbxMaterial::ExtractTextureInfo()`
3. Add material collection methods:
   - `AddAllMaterials()` - Collect all materials from scene
   - `AddAllTextures()` - Collect all textures from scene
   - `LinkMaterialsToMesh()` - Link materials to mesh nodes
4. Update `FBXLoader.cpp` to use `FFbxMaterial`

**Success Criteria**:
- ✅ Build succeeds
- ✅ Materials import correctly
- ✅ Textures resolve to correct paths
- ✅ All material logic is in FFbxMaterial

**Testing**:
- Import FBX with materials (verify albedo, normal, metallic, roughness)
- Import FBX with multiple materials (verify material slots)
- Import FBX with texture paths (verify path resolution)

---

### Phase 4: Extract FFbxAnimation

**Goal**: Separate animation extraction logic

**Files to Create**:
- `Source/Editor/FBX/FbxAnimation.h`
- `Source/Editor/FBX/FbxAnimation.cpp`

**Estimated Size**: ~400 lines

**Tasks**:
1. Create `FFbxAnimation` class
2. Move functions from `FBXLoader`:
   - `LoadAnimationFromStack()` → `FFbxAnimation::ExtractAnimation()`
   - Method signature: `UAnimSequence* ExtractAnimation(FbxScene* Scene, const FSkeleton* Skeleton)`
   - `ExtractBoneAnimationTracks()` → `FFbxAnimation::ExtractBoneCurves()`
   - `ExtractBoneCurve()` → `FFbxAnimation::ExtractCurveData()`
3. Add animation collection methods:
   - `AddSkeletalTransformAnimation()` - Bone animation
   - `AddMorphTargetAnimation()` - Morph target curves (future)
   - `AddRigidAnimation()` - Rigid body animation (future)
4. Update `FBXLoader.cpp` to use `FFbxAnimation`

**Success Criteria**:
- ✅ Build succeeds
- ✅ Skeletal animations import correctly
- ✅ Bone curves extract correctly
- ✅ All animation logic is in FFbxAnimation

**Testing**:
- Import animated FBX (verify animation plays correctly)
- Import FBX with multiple animation takes (verify all takes)
- Import FBX with bone hierarchy (verify all bones animate)

**Important Notes**:

**Animation Caching Architecture**:
- **Binary caching** (.anim.bin files) stays in UFbxLoader (asset serialization layer)
- **FFbxAnimation** provides immediate extraction: `ExtractAnimation(FbxScene*, Skeleton*)`
- **No payload pattern**: Animation is extracted and returned directly
- When .anim.bin cache is valid, FFbxAnimation is bypassed entirely

**Caching Flow**:
```
UFbxLoader::LoadFbxAnimation(FilePath, Skeleton)
├── Check .anim.bin cache (asset-level)
├── If valid → Return cached UAnimSequence*
└── If invalid → FFbxAnimation::ExtractAnimation() → Save to cache
```

---

### Phase 5: Extract FFbxScene

**Goal**: Separate scene hierarchy and skeleton extraction

**Files to Create**:
- `Source/Editor/FBX/FbxScene.h`
- `Source/Editor/FBX/FbxScene.cpp`

**Estimated Size**: ~500 lines

**Tasks**:
1. Create `FFbxScene` class
2. Move functions from `FBXLoader`:
   - `LoadSkeletonFromNode()` → `FFbxScene::ExtractSkeleton()`
   - Method signature: `void ExtractSkeleton(FbxNode* RootNode, FSkeleton& OutSkeleton)`
   - Recursive node traversal logic
   - Joint detection logic
3. Add scene processing methods:
   - `AddHierarchy()` - Main entry point for hierarchy extraction
   - `AddHierarchyRecursively()` - Recursive traversal
   - `CreateTransformNode()` - Create scene nodes
   - `ValidateBindPose()` - Validate skeleton bind pose
4. Update `FBXLoader.cpp` to use `FFbxScene`

**Success Criteria**:
- ✅ Build succeeds
- ✅ Skeleton hierarchy extracts correctly
- ✅ Bone parent-child relationships correct
- ✅ All scene/skeleton logic is in FFbxScene

**Testing**:
- Import skeletal mesh (verify bone hierarchy)
- Import mesh with complex skeleton (verify all bones)
- Import mesh with multiple root bones (verify EnsureSingleRootBone)

---

### Phase 6: Extract FFbxMesh

**Goal**: Isolate mesh data extraction

**Files to Create**:
- `Source/Editor/FBX/FbxMesh.h`
- `Source/Editor/FBX/FbxMesh.cpp`

**Estimated Size**: ~800 lines

**Tasks**:
1. Create `FFbxMesh` class
2. Move functions from `FBXLoader`:
   - `LoadMeshFromNode()` → `FFbxMesh::DiscoverMeshNodes()`
   - `LoadMesh()` → `FFbxMesh::ExtractMeshData()`
   - Method signature: `void ExtractMeshData(FbxMesh* InMesh, FSkeletalMeshData& OutMeshData, const FbxAMatrix& Transform)`
   - Skinning extraction logic
   - Material slot handling
3. Add mesh processing methods:
   - `DiscoverMeshNodes()` - Discover all mesh nodes in scene
   - `ExtractSkinnedMeshNodeJoints()` - Extract skinning data

4. **Note on FMeshDescriptionImporter** (UE5 pattern):
   - UE5 uses this as intermediate layer, but it's tied to FMeshDescription struct
   - For Mundi, we extract directly to FSkeletalMeshData
   - If needed later, create similar helper but NOT in this refactoring

5. **CRITICAL**: Ensure Bone BindPose matrices are set correctly
   ```cpp
   // Lines 682-732 in current FBXLoader.cpp
   for (int Index = 0; Index < Skin->GetClusterCount(); Index++) {
       FbxCluster* Cluster = Skin->GetCluster(Index);
       FbxAMatrix TransformLinkMatrix;
       Cluster->GetTransformLinkMatrix(TransformLinkMatrix);
       // ... JointPostMatrix application
       MeshData.Skeleton.Bones[BoneToIndex[BoneNode]].BindPose = GlobalBindPoseMatrix;
       MeshData.Skeleton.Bones[BoneToIndex[BoneNode]].InverseBindPose = InverseBindPoseMatrix;
   }
   ```

6. Update `FBXLoader.cpp` to use `FFbxMesh`

**Success Criteria**:
- ✅ Build succeeds
- ✅ Meshes import correctly
- ✅ Skinning weights extract correctly
- ✅ **CRITICAL**: Bone BindPose matrices set (meshes visible!)
- ✅ All mesh logic is in FFbxMesh

**Testing**:
- Import skeletal mesh (verify mesh visible and skinning works)
- Import multi-mesh FBX (verify all meshes visible)
- Import mesh with multiple materials (verify material slots)
- **CRITICAL**: Verify meshes are VISIBLE (not collapsed to origin)

---

### Phase 7: Create FFbxParser Orchestrator

**Goal**: Create orchestration layer with immediate loading API

**Files to Create**:
- `Source/Editor/FBX/FbxParser.h`
- `Source/Editor/FBX/FbxParser.cpp`

**Estimated Size**: ~400 lines (single implementation, no interface)

**Tasks**:

1. Create `FFbxParser` class (NO interface, direct implementation)
   ```cpp
   class FFbxParser
   {
   private:
       FFbxScene SceneProcessor;
       FFbxMesh MeshProcessor;
       FFbxAnimation AnimationProcessor;
       FFbxMaterial MaterialProcessor;

       FbxManager* SDKManager;
       FbxScene* SDKScene;
       FbxImporter* SDKImporter;

       // FbxScene* 캐싱 (메시와 애니메이션 임포트 간 공유)
       FCachedFbxScene CachedScene;

   public:
       // IMPORTANT: JointOrientationMatrix is a public member variable (UE5 pattern)
       FbxAMatrix JointOrientationMatrix;  // Set during ConvertScene()

       // 직접 로딩 API (페이로드 패턴 없음)
       USkeletalMesh* LoadFbxMesh(const FString& FilePath);
       UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* Skeleton);

       void Reset();
   };
   ```

2. Implement immediate loading methods:
   ```cpp
   USkeletalMesh* FFbxParser::LoadFbxMesh(const FString& FilePath)
   {
       // FbxScene* 캐싱 활용
       if (!CachedScene.IsValid(FilePath))
       {
           LoadFbxScene(FilePath);  // SDK 초기화 + 씬 로드
       }

       // 즉시 메시 데이터 추출
       FSkeletalMeshData MeshData;
       MeshProcessor.ExtractMeshData(CachedScene.Scene, MeshData);

       // USkeletalMesh 생성 및 반환
       return CreateSkeletalMeshFromData(MeshData);
   }

   UAnimSequence* FFbxParser::LoadFbxAnimation(const FString& FilePath, const FSkeleton* Skeleton)
   {
       // FbxScene* 캐싱 활용 (메시 임포트 후 애니메이션 로드 시)
       if (!CachedScene.IsValid(FilePath))
       {
           LoadFbxScene(FilePath);
       }

       // 즉시 애니메이션 추출
       return AnimationProcessor.ExtractAnimation(CachedScene.Scene, Skeleton);
   }
   ```

3. **Remove ALL payload context code**:
   - No `FPayloadContextBase`
   - No `FMeshPayloadContext`
   - No `FAnimationPayloadContext`
   - No `TMap<FString, TSharedPtr<FPayloadContextBase>> PayloadContexts`
   - No `FetchMeshPayload()` or `FetchAnimationPayload()` methods

4. Refactor `UFbxLoader` to be thin wrapper:
   ```cpp
   class UFbxLoader : public UObject
   {
   private:
       TUniquePtr<FFbxParser> Parser;

   public:
       UFbxLoader() : Parser(MakeUnique<FFbxParser>()) {}

       USkeletalMesh* LoadFbxMesh(const FString& FilePath)
       {
           // Asset-level cache check (.bin file)
           if (USkeletalMesh* Cached = CheckMeshCache(FilePath))
               return Cached;

           // 직접 로드 (페이로드 시스템 없음)
           USkeletalMesh* Mesh = Parser->LoadFbxMesh(FilePath);

           // Save to cache
           SaveMeshCache(FilePath, Mesh);
           return Mesh;
       }

       UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* Skeleton)
       {
           // Asset-level cache check (.anim.bin file)
           if (UAnimSequence* Cached = CheckAnimationCache(FilePath))
               return Cached;

           // 직접 로드
           UAnimSequence* Anim = Parser->LoadFbxAnimation(FilePath, Skeleton);

           // Save to cache
           SaveAnimationCache(FilePath, Anim);
           return Anim;
       }
   };
   ```

5. **Implement FbxScene* caching in FFbxParser** (SDK-level, not asset-level)
   ```cpp
   struct FCachedFbxScene
   {
       FString FilePath;
       FbxScene* Scene;
       uint64 Timestamp;

       bool IsValid(const FString& InFilePath) const
       {
           return FilePath == InFilePath && Scene != nullptr;
       }
   };
   ```

6. **Clarify caching layer architecture**:
   ```
   // ============================================
   // Layer 1: Asset Serialization (UFbxLoader)
   // ============================================
   - Mesh caching: .bin files (timestamp-based invalidation)
   - Animation caching: .anim.bin files (timestamp-based invalidation)
   - Purpose: Skip entire FBX parsing + extraction pipeline

   // ============================================
   // Layer 2: FBX SDK Management (FFbxParser)
   // ============================================
   - In-memory FbxScene* caching (temporary, single scene)
   - Purpose: Share FbxScene between mesh and animation import
   - Lifetime: Until different file loaded or parser destroyed
   - NO payload pattern, immediate extraction
   ```

**Success Criteria**:
- ✅ Build succeeds
- ✅ `UFbxLoader` is now a thin wrapper (~200 lines)
- ✅ All FBX SDK management is in `FFbxParser`
- ✅ Clean orchestration of specialized classes
- ✅ No payload contexts, immediate loading only
- ✅ FbxScene* caching works correctly

**Testing**:
- Import skeletal mesh (verify works through new API)
- Import animation (verify works through new API)
- Import multiple files sequentially (verify cleanup works)

---

## Implementation Guidelines

### General Principles

1. **One Phase at a Time**
   - Complete one phase fully before starting next
   - Build and test after each phase
   - Commit after each successful phase

2. **Maintain Functionality**
   - Existing FBX imports must work identically after each phase
   - No visual changes to imported meshes/animations
   - No performance regressions

3. **Follow UE5 Patterns**
   - Study UE5 code before implementing each phase
   - Match UE5's class structure and method names where possible
   - Use UE5's architectural patterns (payload contexts, templates)

4. **Comprehensive Testing**
   - Test with multiple FBX files after each phase
   - Test skeletal meshes with animations
   - Test multi-mesh FBX files
   - Test multi-material FBX files

5. **Documentation**
   - Add comprehensive comments to each new class
   - Document UE5 patterns being used
   - Explain why specific design decisions were made

### Code Style

**IMPORTANT: Korean Comment Policy**
- **All code comments MUST be written in Korean (한국어)**
- This applies to:
  - File-level documentation comments
  - Class/struct documentation comments
  - Method/function documentation comments
  - Inline comments explaining logic
  - Step-by-step process comments
- Exception: UE5 pattern references and technical terms can use English within Korean comments
- Example:
  ```cpp
  /**
   * FFbxConvert
   *
   * UE5 스타일의 FBX 데이터 변환 유틸리티 (템플릿 기반 정밀도 지원)
   * 좌표계 변환(오른손 → 왼손)을 Y-Flip 방식으로 처리
   */
  ```

**Header File Template**:
```cpp
#pragma once
#include "fbxsdk.h"
#include "Vector.h"
#include "Quat.h"
#include "Matrix.h"

/**
 * FFbxClassName
 *
 * [클래스 역할에 대한 간단한 설명]
 *
 * UE5 Pattern: [대응하는 UE5 클래스]
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/[FileName].cpp
 */
class FFbxClassName
{
public:
    // Public API 메서드

private:
    // Private 헬퍼 메서드
};
```

**Method Documentation**:
```cpp
/**
 * ExtractMeshData
 *
 * FbxMesh에서 정점, 삼각형, 머티리얼 데이터를 추출
 *
 * UE5 Pattern: FMeshDescriptionImporter::FillSkinnedMeshDescriptionFromFbxMesh
 * Location: FbxMesh.cpp:1847
 *
 * @param InMesh - 데이터를 추출할 FBX 메시 노드
 * @param OutMeshData - 출력 스켈레탈 메시 데이터 구조
 * @param Transform - TotalMatrix (GlobalTransform * GeometryTransform)
 */
void ExtractMeshData(FbxMesh* InMesh, FSkeletalMeshData& OutMeshData, const FbxAMatrix& Transform);
```

**Critical Sections**:
```cpp
// 중요: 여기서 Bone BindPose 행렬을 반드시 설정해야 함!
// 이것 없이는 GPU 스키닝이 실패하고 메시가 보이지 않음.
// UE5 Pattern: FbxSkeletalMeshImport.cpp:2156-2189
for (int Index = 0; Index < Skin->GetClusterCount(); Index++)
{
    // ... BindPose 추출
}
```

### Testing Checklist

**After Each Phase**:
- [ ] Build succeeds with no errors or warnings
- [ ] Import test_skeletal_mesh.fbx - verify mesh visible
- [ ] Import test_skeletal_mesh.fbx - verify skeleton hierarchy correct
- [ ] Import test_animation.fbx - verify animation plays correctly
- [ ] Import test_multi_mesh.fbx - verify all meshes visible
- [ ] Import test_multi_material.fbx - verify all materials apply
- [ ] No 100x scale bugs
- [ ] No "lying down" orientation bugs
- [ ] Bone BindPose matrices are set (check in debugger)

**Final Testing** (after Phase 7):
- [ ] All individual phase tests pass
- [ ] Import 10+ different FBX files
- [ ] Test with FBX from Blender, Maya, 3ds Max
- [ ] Test with static meshes (if supported)
- [ ] Test with morph targets (if supported)
- [ ] Performance is same or better than before
- [ ] Memory usage is same or better than before

---

## Known Pitfalls

### Pitfall 1: Forgetting Bone BindPose Matrices

**Problem**: When extracting mesh data separately, easy to forget to set Bone BindPose matrices.

**Symptom**: Meshes completely invisible (all vertices collapse to origin).

**Solution**:
```cpp
// In FFbxMesh::ExtractMeshData(), ensure this code runs:
for (int Index = 0; Index < Skin->GetClusterCount(); Index++)
{
    FbxCluster* Cluster = Skin->GetCluster(Index);
    FbxAMatrix TransformLinkMatrix;
    Cluster->GetTransformLinkMatrix(TransformLinkMatrix);

    // Apply JointOrientationMatrix (from Parser, set during ConvertScene)
    // IMPORTANT: JointOrientationMatrix is a member variable, NOT a function!
    FbxAMatrix& JointPostMatrix = Parser.JointOrientationMatrix;
    FbxAMatrix GlobalBindPoseMatrix = TransformLinkMatrix * JointPostMatrix;
    FbxAMatrix InverseBindPoseMatrix = GlobalBindPoseMatrix.Inverse();

    // CRITICAL: Set BindPose matrices!
    MeshData.Skeleton.Bones[BoneIndex].BindPose = FFbxConvert::ConvertMatrix(GlobalBindPoseMatrix);
    MeshData.Skeleton.Bones[BoneIndex].InverseBindPose = FFbxConvert::ConvertMatrix(InverseBindPoseMatrix);
}
```

### Pitfall 2: Matrix M[1][1] Negation

**Problem**: Negating M[1][1] when converting matrix (causes 100x scale bugs).

**Solution**: UE5 keeps M[1][1] unchanged!
```cpp
// CORRECT (UE5 Pattern):
Result.M[1][0] = -Result.M[1][0];
Result.M[1][1] = Result.M[1][1];  // NOT negated!
Result.M[1][2] = -Result.M[1][2];
Result.M[1][3] = -Result.M[1][3];
```

### Pitfall 3: Missing TotalMatrix Computation

**Problem**: Using Cluster->GetTransformMatrix() instead of ComputeSkeletalMeshTotalMatrix().

**Solution**: Always use TotalMatrix = GlobalTransform * GeometryTransform.
```cpp
FbxAMatrix TotalMatrix = ComputeSkeletalMeshTotalMatrix(MeshNode, RootNode);
FbxVector4 FinalPosition = TotalMatrix.MultT(Position);
```

### Pitfall 4: Wrong Default Coordinate System

**Problem**: Using `+X Forward` as default instead of UE5's `-Y Forward`.

**Solution**: Set `bForceFrontXAxis = false` by default.
```cpp
bool bForceFrontXAxis = false;  // UE5 default: -Y Forward
```

### Pitfall 5: IndexMap Scope in Multi-Mesh

**Problem**: When processing multiple meshes, IndexMap scope must be managed carefully.

**Solution**: Use local IndexMap per mesh for deduplication, global offset for merging.
```cpp
// Per-mesh deduplication
TMap<FString, int32> LocalIndexMap;
// ... deduplicate vertices

// Global merge with offset
int32 VertexOffset = MergedVertices.Num();
MergedVertices.Append(MeshVertices);
for (int32 Index : MeshIndices)
{
    MergedIndices.Add(Index + VertexOffset);
}
```

---

## Success Metrics

### Code Quality
- [ ] Total lines in `UFbxLoader` reduced from 1839 to ~200
- [ ] Each specialized class < 1000 lines
- [ ] Each class has single, well-defined responsibility
- [ ] Zero code duplication between classes
- [ ] All functions have clear, descriptive names

### Functionality
- [ ] All existing FBX imports work identically
- [ ] No visual changes to imported meshes
- [ ] No visual changes to animations
- [ ] All materials apply correctly
- [ ] All bone hierarchies extract correctly

### Maintainability
- [ ] Easy to find where mesh extraction happens (FFbxMesh)
- [ ] Easy to find where animation extraction happens (FFbxAnimation)
- [ ] Easy to find where coordinate conversion happens (FFbxConvert)
- [ ] Each class can be tested independently
- [ ] New features can be added to appropriate class

### Extensibility
- [ ] Can add morph targets by extending FFbxMesh
- [ ] Can add static mesh import by extending FFbxMesh
- [ ] Can add alternative parser (ufbx) if needed in future (would require new design)
- [ ] FbxScene* caching allows efficient mesh+animation imports from same file
- [ ] Modular design allows easy addition of new extraction features

---

## References

### UE5 Source Files
- `C:\Dev\UE5\UnrealEngine\Engine\Plugins\Interchange\Runtime\Source\Parsers\Fbx\`
  - `Private/FbxAPI.h/.cpp` - Parser implementation (uses IFbxParser interface and payload pattern)
  - `Private/FbxScene.h/.cpp` - Scene hierarchy
  - `Private/FbxMesh.h/.cpp` - Mesh extraction
  - `Private/FbxAnimation.h/.cpp` - Animation extraction
  - `Private/FbxMaterial.h/.cpp` - Material/texture
  - `Private/FbxConvert.h/.cpp` - Coordinate conversion
  - `Private/FbxHelper.h/.cpp` - Utilities

**Note**: UE5's Interchange plugin uses payload contexts for lazy loading and IFbxParser
interface for plugin architecture. Mundi's refactoring adopts the modularization
(FFbxConvert, FFbxHelper, etc.) but NOT the payload/interface patterns for simplicity.

### Mundi Current Files
- `Mundi/Source/Editor/FBXLoader.h/.cpp` - Current monolithic implementation
- `Mundi/Source/Editor/FbxDataConverter.h/.cpp` - Current conversion utilities
- `Mundi/Docs/UE5_vs_Mundi_FBX_Import_Analysis.md` - Detailed analysis document

### Related Documentation
- `Mundi/Docs/FBX_Import_Migration_Plan.md` - Original migration plan (Week10 → Week11_4)
- FBX SDK Documentation: https://help.autodesk.com/view/FBX/2020/ENU/

---

## Revision History

- **2025-11-16 (v2.0)**: Removed lazy loading (payload) features for simplified architecture
  - **REMOVED**: FPayloadContextBase, FMeshPayloadContext, FAnimationPayloadContext
  - **REMOVED**: IFbxParser interface pattern (Strategy pattern)
  - **REMOVED**: LoadFbxFile() + FetchPayload() separation pattern
  - **CHANGED**: To immediate loading API (LoadFbxMesh, LoadFbxAnimation return directly)
  - **KEPT**: All code modularization (FFbxConvert, FFbxHelper, FFbxMaterial, FFbxAnimation, FFbxScene, FFbxMesh, FFbxParser)
  - **KEPT**: FbxScene* caching in FFbxParser (SDK-level, not payload-based)
  - Updated all phase descriptions to reflect direct loading
  - Simplified architecture: easier implementation and maintenance
  - Preserved all critical sections (BindPose, M[1][1], coordinate system)
  - Preserved Korean comment policy and testing checklist

- **2025-11-15 (v1.2)**: Corrected UE5 implementation details (accuracy 90% → 99%)
  - **CRITICAL FIX**: Removed `GetJointPostConversionMatrix()` function (does not exist in UE5)
    - Updated Phase 1: JointOrientationMatrix is a member variable of FFbxParser, not a function
    - Updated Known Pitfalls: Changed code examples to use `Parser.JointOrientationMatrix`
  - Updated Phase 1, 2: FFbxConvert and FFbxHelper are `struct`, not `class`
  - Updated Phase 2: Added note that FPayloadContextBase is defined in FbxHelper.h
  - Updated Phase 6: Added FMeshDescriptionImporter helper class documentation
  - Updated Phase 7: Removed PayloadContexts.h file creation (contexts are co-located)
  - Updated Phase 7: Added JointOrientationMatrix as public member variable in FFbxParser
  - Verified against actual UE5 source code in C:\Dev\UE5\UnrealEngine\Engine\Plugins\Interchange

- **2025-11-15 (v1.1)**: Updated for animation caching
  - Added clarification in Phase 4: Animation caching vs FFbxAnimation separation
  - Updated Phase 7 task 5: Clarified SDK-level vs asset-level caching
  - Added Phase 7 task 6: Caching layer architecture diagram
  - Reflected newly added .anim.bin caching implementation (commit 28b3730e)
  - Animation caching stays in UFbxLoader, FbxScene* caching moves to FFbxParser

- **2025-11-15 (v1.0)**: Initial document creation
  - Analyzed UE5 Interchange plugin architecture
  - Defined 7-phase refactoring plan
  - Documented known pitfalls and success criteria
