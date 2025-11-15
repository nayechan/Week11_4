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

### UE5's Solution
- Distributes ~6000 lines across 6+ specialized classes
- Each class has a single, well-defined responsibility
- Uses Payload Context pattern for lazy loading
- Template-based coordinate conversion
- Strategy pattern allows swapping FBX SDK for alternatives (ufbx)

### Target Architecture

```
Mundi/Source/Editor/FBX/
├── FbxConvert.h/.cpp        # Coordinate conversion utilities (~200 lines)
├── FbxHelper.h/.cpp         # Naming and ID generation (~300 lines)
├── FbxMaterial.h/.cpp       # Material/texture extraction (~300 lines)
├── FbxAnimation.h/.cpp      # Animation processing (~400 lines)
├── FbxScene.h/.cpp          # Scene hierarchy and skeleton (~500 lines)
├── FbxMesh.h/.cpp           # Mesh extraction and skinning (~800 lines)
├── FbxAPI.h/.cpp            # FFbxParser implementation (~400 lines)
├── IFbxParser.h             # Abstract interface
└── PayloadContexts.h        # Payload context definitions

Mundi/Source/Editor/
└── FBXLoader.h/.cpp         # Thin wrapper using FFbxParser
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

**FFbxHelper** (~300 lines)
- Mesh name/ID generation
- Node hierarchy name generation
- FBX namespace handling
- Name clash resolution
- Material name sanitization

**FFbxMaterial** (443 lines)
- Texture extraction and node creation
- Material property parsing
- Material-to-mesh node linking
- Texture path resolution
- Material instance creation

**FFbxAnimation** (1191 lines)
- Skeletal bone animation extraction
- Morph target animation curves
- Rigid body animation
- Custom property animation
- Animation curve baking
- Animation payload contexts

**FFbxScene** (1312 lines)
- Scene hierarchy traversal
- Skeleton structure extraction
- Joint detection and validation
- Bind pose validation
- Geometric transform extraction
- Recursive node processing

**FFbxMesh** (2388 lines)
- Mesh discovery and node creation
- Vertex/triangle extraction
- Skinning data extraction (clusters, weights)
- Static mesh processing
- Skeletal mesh processing
- Morph target extraction
- Bind pose matrix calculation
- Material slot handling

**FFbxParser** (466 lines)
- FBX SDK initialization and management
- Scene loading and conversion
- Orchestrates all specialized classes
- Provides high-level import API
- Handles cleanup and lifecycle

### Key Architectural Patterns

#### 1. Single Responsibility Principle
Each class has ONE clear job. No mixing of concerns.

#### 2. Payload Context Pattern
```cpp
// During scene traversal, create lightweight context
TSharedPtr<FMeshPayloadContext> Context = MakeShared<FMeshPayloadContext>();
Context->Mesh = FbxMesh;
Context->SDKScene = SDKScene;
PayloadContexts.Add(PayloadKey, Context);

// Later, when mesh is actually needed (lazy loading)
if (TSharedPtr<FPayloadContextBase> Context = PayloadContexts.Find(PayloadKey))
{
    Context->FetchMeshPayload(Parser, Transform, OutMeshDescription);
}
```

**Benefits**:
- Memory efficient (don't load all meshes at once)
- Supports multi-threaded import
- Allows progressive loading
- Clean separation of structure vs data

#### 3. Strategy Pattern
```cpp
IFbxParser (Abstract Interface)
├── FFbxParser (FBX SDK implementation)
└── FUfbxParser (ufbx library implementation)
```

**Benefits**:
- Can swap FBX SDK for alternatives
- Easier to test with mock implementations
- Future-proof for new libraries

#### 4. Template-Based Conversion
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
1. Create `FFbxConvert` class with static methods
2. Move functions from `FbxDataConverter`:
   - `ConvertPos()` - Position conversion (Y-Flip)
   - `ConvertDir()` - Direction conversion (Y-Flip + normalize)
   - `ConvertRotation()` - Quaternion conversion (Y/W negation)
   - `ConvertScale()` - Scale conversion (no Y-Flip)
   - `ConvertFbxMatrixWithYAxisFlip()` - Matrix conversion (preserve M[1][1]!)
   - `GetJointPostConversionMatrix()` - Joint post-conversion
3. Make template-based for future precision support
4. Update `FBXLoader.cpp` to use `FFbxConvert::` instead of `FFbxDataConverter::`
5. Add comprehensive comments explaining Y-Flip logic

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

**Estimated Size**: ~300 lines

**Tasks**:
1. Create `FFbxHelper` class with static methods
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
   - `LoadAnimationFromStack()` → `FFbxAnimation::ExtractAnimationStack()`
   - `ExtractBoneAnimationTracks()` → `FFbxAnimation::ExtractBoneCurves()`
   - `ExtractBoneCurve()` → `FFbxAnimation::ExtractCurveData()`
3. Add animation collection methods:
   - `AddSkeletalTransformAnimation()` - Bone animation
   - `AddMorphTargetAnimation()` - Morph target curves (future)
   - `AddRigidAnimation()` - Rigid body animation (future)
4. Create `FAnimationPayloadContext` for deferred animation loading
5. Update `FBXLoader.cpp` to use `FFbxAnimation`

**Success Criteria**:
- ✅ Build succeeds
- ✅ Skeletal animations import correctly
- ✅ Bone curves extract correctly
- ✅ All animation logic is in FFbxAnimation

**Testing**:
- Import animated FBX (verify animation plays correctly)
- Import FBX with multiple animation takes (verify all takes)
- Import FBX with bone hierarchy (verify all bones animate)

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
   - `LoadSkeletonFromNode()` → `FFbxScene::AddSkeletonHierarchy()`
   - Recursive node traversal logic
   - Joint detection logic
3. Add scene processing methods:
   - `AddHierarchy()` - Main entry point for hierarchy extraction
   - `AddHierarchyRecursively()` - Recursive traversal
   - `CreateTransformNode()` - Create scene nodes
   - `ValidateBindPose()` - Validate skeleton bind pose
4. Create `FSkeletonPayloadContext` for deferred skeleton building
5. Update `FBXLoader.cpp` to use `FFbxScene`

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
   - `LoadMeshFromNode()` → `FFbxMesh::AddAllMeshes()`
   - `LoadMesh()` → `FFbxMesh::ExtractMeshData()`
   - Skinning extraction logic
   - Material slot handling
3. Add mesh processing methods:
   - `AddAllMeshes()` - Discover all mesh nodes in scene
   - `ExtractSkinnedMeshNodeJoints()` - Extract skinning data
   - `FillSkinnedMeshData()` - Fill skeletal mesh data
   - `FillStaticMeshData()` - Fill static mesh data (future)
   - `ExtractMorphTargets()` - Extract morph targets (future)
4. Create `FMeshPayloadContext` for deferred mesh loading
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

### Phase 7: Create FFbxParser Facade

**Goal**: Create clean public API and orchestration layer

**Files to Create**:
- `Source/Editor/FBX/IFbxParser.h` (interface)
- `Source/Editor/FBX/FbxAPI.h`
- `Source/Editor/FBX/FbxAPI.cpp`
- `Source/Editor/FBX/PayloadContexts.h`

**Estimated Size**: ~400 lines

**Tasks**:
1. Create `IFbxParser` abstract interface
   ```cpp
   class IFbxParser
   {
   public:
       virtual ~IFbxParser() = default;
       virtual bool LoadFbxFile(const FString& FilePath) = 0;
       virtual FSkeletalMeshData* FetchMeshPayload(const FString& PayloadKey) = 0;
       virtual UAnimSequence* FetchAnimationPayload(const FString& PayloadKey) = 0;
       virtual void Reset() = 0;
   };
   ```

2. Create `FFbxParser` implementation
   ```cpp
   class FFbxParser : public IFbxParser
   {
   private:
       FFbxScene SceneProcessor;
       FFbxMesh MeshProcessor;
       FFbxAnimation AnimationProcessor;
       FFbxMaterial MaterialProcessor;

       FbxManager* SDKManager;
       FbxScene* SDKScene;
       FbxImporter* SDKImporter;

       TMap<FString, TSharedPtr<FPayloadContextBase>> PayloadContexts;

   public:
       bool LoadFbxFile(const FString& FilePath) override;
       FSkeletalMeshData* FetchMeshPayload(const FString& PayloadKey) override;
       UAnimSequence* FetchAnimationPayload(const FString& PayloadKey) override;
       void Reset() override;
   };
   ```

3. Create payload context base classes:
   ```cpp
   class FPayloadContextBase
   {
   public:
       virtual ~FPayloadContextBase() = default;
   };

   class FMeshPayloadContext : public FPayloadContextBase
   {
   public:
       FbxMesh* Mesh;
       FbxScene* SDKScene;
       FbxNode* MeshNode;

       FSkeletalMeshData* FetchMeshPayload(FFbxParser* Parser);
   };

   class FAnimationPayloadContext : public FPayloadContextBase
   {
   public:
       FbxAnimStack* AnimStack;
       FbxScene* SDKScene;

       UAnimSequence* FetchAnimationPayload(FFbxParser* Parser, const FSkeleton* TargetSkeleton);
   };
   ```

4. Refactor `UFbxLoader` to be thin wrapper:
   ```cpp
   class UFbxLoader : public UObject
   {
   private:
       TUniquePtr<IFbxParser> Parser;

   public:
       UFbxLoader() : Parser(MakeUnique<FFbxParser>()) {}

       USkeletalMesh* LoadFbxMesh(const FString& FilePath)
       {
           Parser->LoadFbxFile(FilePath);
           return Parser->FetchMeshPayload("DefaultMesh");
       }

       UAnimSequence* LoadFbxAnimation(const FString& FilePath, const FSkeleton* Skeleton)
       {
           Parser->LoadFbxFile(FilePath);
           return Parser->FetchAnimationPayload("DefaultAnim", Skeleton);
       }
   };
   ```

5. Remove manual scene caching from `UFbxLoader` (let upper layers handle)

**Success Criteria**:
- ✅ Build succeeds
- ✅ `UFbxLoader` is now a thin wrapper (~200 lines)
- ✅ All FBX SDK management is in `FFbxParser`
- ✅ Clean separation between interface and implementation

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
 * [Brief description of responsibility]
 *
 * UE5 Pattern: [Which UE5 class this corresponds to]
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/[FileName].cpp
 */
class FFbxClassName
{
public:
    // Public API methods

private:
    // Private helper methods
};
```

**Method Documentation**:
```cpp
/**
 * ExtractMeshData
 *
 * Extracts vertex, triangle, and material data from FbxMesh.
 *
 * UE5 Pattern: FMeshDescriptionImporter::FillSkinnedMeshDescriptionFromFbxMesh
 * Location: FbxMesh.cpp:1847
 *
 * @param InMesh - FBX mesh node to extract data from
 * @param OutMeshData - Output skeletal mesh data structure
 * @param Transform - TotalMatrix (GlobalTransform * GeometryTransform)
 */
void ExtractMeshData(FbxMesh* InMesh, FSkeletalMeshData& OutMeshData, const FbxAMatrix& Transform);
```

**Critical Sections**:
```cpp
// CRITICAL: Must set Bone BindPose matrices here!
// Without this, GPU skinning will fail and meshes will be invisible.
// UE5 Pattern: FbxSkeletalMeshImport.cpp:2156-2189
for (int Index = 0; Index < Skin->GetClusterCount(); Index++)
{
    // ... extract BindPose
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

    // Apply JointPostConversionMatrix if needed
    FbxAMatrix JointPostMatrix = FFbxConvert::GetJointPostConversionMatrix(bForceFrontXAxis);
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
- [ ] Can add ufbx implementation by implementing IFbxParser
- [ ] Can add payload pattern for lazy loading (foundation in place)
- [ ] Can add multi-threading support (payload contexts are thread-safe)
- [ ] Can add morph targets by extending FFbxMesh
- [ ] Can add static mesh import by extending FFbxMesh

---

## References

### UE5 Source Files
- `C:\Dev\UE5\UnrealEngine\Engine\Plugins\Interchange\Runtime\Source\Parsers\Fbx\`
  - `Private/FbxAPI.h/.cpp` - Parser implementation
  - `Private/FbxScene.h/.cpp` - Scene hierarchy
  - `Private/FbxMesh.h/.cpp` - Mesh extraction
  - `Private/FbxAnimation.h/.cpp` - Animation extraction
  - `Private/FbxMaterial.h/.cpp` - Material/texture
  - `Private/FbxConvert.h/.cpp` - Coordinate conversion
  - `Private/FbxHelper.h/.cpp` - Utilities

### Mundi Current Files
- `Mundi/Source/Editor/FBXLoader.h/.cpp` - Current monolithic implementation
- `Mundi/Source/Editor/FbxDataConverter.h/.cpp` - Current conversion utilities
- `Mundi/Docs/UE5_vs_Mundi_FBX_Import_Analysis.md` - Detailed analysis document

### Related Documentation
- `Mundi/Docs/FBX_Import_Migration_Plan.md` - Original migration plan (Week10 → Week11_4)
- FBX SDK Documentation: https://help.autodesk.com/view/FBX/2020/ENU/

---

## Revision History

- **2025-11-15**: Initial document creation
  - Analyzed UE5 Interchange plugin architecture
  - Defined 7-phase refactoring plan
  - Documented known pitfalls and success criteria
