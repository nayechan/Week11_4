# Skeleton 관리 시스템 구현 계획

> **작성일:** 2025-11-18
> **아키텍처:** Option A - FFbxManager (신규) + UFbxLoader (유지)
> **패턴:** FObjManager 참고

---

## 📋 목차

1. [개요](#개요)
2. [문제 정의](#문제-정의)
3. [아키텍처 설계](#아키텍처-설계)
4. [구현 단계](#구현-단계)
5. [파일별 변경 사항](#파일별-변경-사항)
6. [테스트 계획](#테스트-계획)
7. [참고 자료](#참고-자료)

---

## 개요

### 목적
- **Skeleton 중앙 관리**: FSkeleton 객체를 메모리에 캐싱하여 중복 제거
- **UI 기반 Skeleton 교체**: Property Window에서 드롭다운으로 Skeleton 선택 가능
- **메모리 문제 해결**: 전체 리소스 스캔을 제거하여 100GB+ 메모리 사용 방지
- **포인터 기반 필터링**: AnimToPlay 필터링을 문자열 비교 → 포인터 비교로 변경

### 배경
- A.fbx와 B.fbx가 같은 Skeleton 구조를 가지지만 메모리상 다른 객체
- 문자열 ID 매칭 방식은 모든 리소스를 스캔하여 메모리 폭발 발생
- Skeleton을 중앙에서 관리하여 같은 Skeleton 객체를 공유하도록 변경

---

## 문제 정의

### 현재 문제점

#### 1. 메모리 폭발 (100GB+)
```cpp
// PropertyRenderer.cpp (라인 595-629)
if (FString(Prop.Name) == "SkeletonID")
{
    if (CachedSkeletonIDs.IsEmpty())
    {
        // ⚠️ 모든 SkeletalMesh와 AnimSequence 스캔
        TArray<FString> AllSkeletalMeshPaths = ResMgr.GetAllFilePaths<USkeletalMesh>();
        for (const FString& MeshPath : AllSkeletalMeshPaths)
        {
            USkeletalMesh* Mesh = ResMgr.Get<USkeletalMesh>(MeshPath);
            // 100+ 리소스 접근 시 메모리 폭발
        }
    }
}
```

#### 2. Skeleton 중복
- A.fbx와 B.fbx가 같은 Skeleton이지만 독립적인 `FSkeleton` 객체
- 포인터 비교 시 다르다고 판단 (`Anim->Skeleton != Mesh->GetSkeleton()`)

#### 3. 수동 교체 불가
- Skeleton 이름이 다르면 통합 불가능
- UI에서 Skeleton을 선택하여 교체할 수 없음

### 요구사항
1. ✅ Skeleton을 중앙에서 관리 (FObjManager 패턴)
2. ✅ 같은 Skeleton 이름 → 같은 포인터 반환 (중복 제거)
3. ✅ UI에서 Skeleton 선택하여 교체 가능
4. ✅ Skeleton 교체 시 SkeletalMesh와 AnimToPlay 동시 적용
5. ✅ 메모리 효율적 (전체 리소스 스캔 제거)

---

## 아키텍처 설계

### 계층 구조

```
┌─────────────────────────────────────────────┐
│ SkeletalMeshComponentCustomization (UI)    │
│ - Skeleton 드롭다운 렌더링                  │
│ - Skeleton 교체 처리                        │
│ - SkeletalMesh + AnimToPlay 동시 교체       │
└─────────────────────────────────────────────┘
                    ↓ uses
┌─────────────────────────────────────────────┐
│ FFbxManager (신규 - Manager 역할)          │
│ ├─ TMap<FString, FSkeleton*> 메모리 캐시   │
│ ├─ GetSkeleton() - 캐시된 Skeleton 반환    │
│ ├─ RegisterSkeleton() - Skeleton 등록      │
│ └─ GetAllSkeletonNames() - 드롭다운용 목록 │
└─────────────────────────────────────────────┘
                    ↓ delegates to
┌─────────────────────────────────────────────┐
│ UFbxLoader (기존 유지 - Loader 역할)       │
│ ├─ 디스크 캐싱 (.bin 파일)                  │
│ ├─ LoadFbxMeshAsset() - 파싱 + 디스크 캐시 │
│ └─ FFbxParser에 위임                        │
└─────────────────────────────────────────────┘
                    ↓ delegates to
┌─────────────────────────────────────────────┐
│ FFbxParser (기존 유지 - Parser 역할)        │
│ ├─ FBX SDK 관리                             │
│ └─ 실제 FBX 파싱 수행                       │
└─────────────────────────────────────────────┘
```

### 참고: FObjManager 패턴

```cpp
// ObjManager.h (참고용)
class FObjManager
{
private:
    static TMap<FString, FStaticMesh*> ObjStaticMeshMap;  // 메모리 캐시
public:
    static FStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);
    static void RegisterStaticMeshAsset(const FString& PathFileName, FStaticMesh* InStaticMesh);
};
```

**FFbxManager는 FObjManager와 동일한 패턴 적용**

### 데이터 흐름

#### FBX 로딩
```
1. USkeletalMesh::Load() 호출
   └─> FFbxManager::LoadSkeletalMeshAsset()
       ├─ 메모리 캐시 확인
       │  └─ 있으면: 캐시된 포인터 반환
       └─ 없으면:
          ├─ UFbxLoader::LoadFbxMeshAsset() 호출
          ├─ FFbxManager에 등록
          └─ 포인터 반환
```

#### Skeleton 교체 (UI)
```
1. 사용자가 Property Window에서 Skeleton 드롭다운 클릭
2. FFbxManager::GetAllSkeletonNames() 호출
3. 드롭다운 목록 표시
4. 사용자가 "Character_Skeleton" 선택
5. SkeletalMeshComponentCustomization::OnSkeletonChanged() 호출
   ├─ SkeletalMesh->SetSkeleton(newSkeleton)
   └─ AnimToPlay->Skeleton = newSkeleton
6. AnimToPlay 필터링 자동 업데이트 (포인터 비교)
```

---

## 구현 단계

### Phase 1: FFbxManager 생성 ⏱️ 2-3시간

#### Step 1.1: FbxManager.h 생성
**파일:** `Mundi/Source/Editor/FbxManager.h`

- [ ] 헤더 가드 및 include 추가
- [ ] FFbxManager 클래스 선언
- [ ] TMap 정적 멤버 변수 선언
- [ ] 공개 API 함수 선언

**체크포인트:** 헤더 파일 컴파일 성공

#### Step 1.2: FbxManager.cpp 생성
**파일:** `Mundi/Source/Editor/FbxManager.cpp`

- [ ] 정적 멤버 변수 초기화
- [ ] GetSkeleton() 구현
- [ ] RegisterSkeleton() 구현
- [ ] GetAllSkeletonNames() 구현
- [ ] LoadSkeletalMeshAsset() 구현
- [ ] Clear() 구현

**체크포인트:** 컴파일 성공

#### Step 1.3: Mundi.vcxproj 업데이트
**파일:** `Mundi/Mundi.vcxproj`

- [ ] `<ClCompile Include="Source\Editor\FbxManager.cpp" />` 추가
- [ ] `<ClInclude Include="Source\Editor\FbxManager.h" />` 추가

**체크포인트:** 프로젝트 빌드 성공

---

### Phase 2: FSkeletalMesh 구조 수정 ⏱️ 1-2시간

#### Step 2.1: FSkeleton → FSkeleton* 변경
**파일:** `Mundi/Source/Runtime/Core/Misc/VertexData.h`

- [ ] `FSkeleton Skeleton;` → `FSkeleton* Skeleton = nullptr;` 변경
- [ ] `GetSkeleton()` 헬퍼 함수 추가
- [ ] Serialization 로직 수정 (Skeleton 이름 저장/로드)

**체크포인트:** VertexData.h 컴파일 성공

#### Step 2.2: USkeletalMesh::GetSkeleton() 수정
**파일:** `Mundi/Source/Runtime/AssetManagement/SkeletalMesh.h`

- [ ] `return Data ? &Data->Skeleton : nullptr;` → `return Data ? Data->Skeleton : nullptr;` 변경
- [ ] `SetSkeleton(FSkeleton* NewSkeleton)` 함수 추가
- [ ] `GetBoneCount()` 수정 (`Data->Skeleton.Bones` → `Data->Skeleton->Bones`)

**체크포인트:** SkeletalMesh.h 컴파일 성공

---

### Phase 3: UFbxLoader 통합 ⏱️ 1-2시간

#### Step 3.1: LoadFbxMeshAsset() 수정
**파일:** `Mundi/Plugins/Fbx/FbxLoader.cpp`

- [ ] Skeleton 이름 추출 로직 추가
- [ ] `FFbxManager::RegisterSkeleton()` 호출 추가
- [ ] `FSkeletalMesh->Skeleton` 포인터 설정

**체크포인트:** FBX 로딩 테스트 성공

#### Step 3.2: LoadAnimationAsset() 수정
**파일:** `Mundi/Plugins/Fbx/FbxLoader.cpp`

- [ ] Skeleton 이름 추출
- [ ] `FFbxManager::GetSkeleton()` 호출로 Skeleton 가져오기
- [ ] `UAnimSequence->Skeleton` 포인터 설정

**체크포인트:** 애니메이션 로딩 테스트 성공

---

### Phase 4: PropertyRenderer 정리 ⏱️ 30분

#### Step 4.1: SkeletonID 드롭다운 제거
**파일:** `Mundi/Source/Slate/Widgets/PropertyRenderer.cpp`

- [ ] `RenderStringProperty()` 함수에서 라인 592-662 제거
- [ ] `CachedSkeletonIDs` 멤버 변수 제거 (PropertyRenderer.h)
- [ ] `CachedSkeletonIDs` 초기화 코드 제거 (PropertyRenderer.cpp 라인 39)

**체크포인트:** PropertyRenderer 컴파일 성공

---

### Phase 5: UI 통합 (Skeleton 선택 드롭다운) ⏱️ 2-3시간

#### Step 5.1: SkeletalMeshComponentCustomization.h 수정
**파일:** `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.h`

- [ ] `FString TargetSkeletonID;` → `FSkeleton* TargetSkeleton = nullptr;` 변경
- [ ] `USkeletalMeshComponent* CurrentComponent = nullptr;` 추가
- [ ] `void RenderSkeletonSelector();` 함수 선언 추가
- [ ] `void OnSkeletonChanged(FSkeleton* NewSkeleton);` 함수 선언 추가

**체크포인트:** 헤더 컴파일 성공

#### Step 5.2: CustomizeDetails() 수정
**파일:** `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.cpp`

- [ ] Skeleton 포인터 추출 로직으로 변경
- [ ] `RenderSkeletonSelector()` 호출 추가
- [ ] 필터링 로직을 포인터 비교로 변경

**체크포인트:** CustomizeDetails 동작 확인

#### Step 5.3: RenderSkeletonSelector() 구현
**파일:** `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.cpp`

- [ ] ImGui::Combo로 드롭다운 렌더링
- [ ] `FFbxManager::GetAllSkeletonNames()` 호출
- [ ] 선택 시 `OnSkeletonChanged()` 호출

**체크포인트:** 드롭다운 UI 표시 확인

#### Step 5.4: OnSkeletonChanged() 구현
**파일:** `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.cpp`

- [ ] SkeletalMesh의 Skeleton 포인터 교체
- [ ] AnimToPlay의 Skeleton 포인터 교체
- [ ] SkeletonID 문자열 업데이트
- [ ] 로깅 추가

**체크포인트:** Skeleton 교체 동작 확인

#### Step 5.5: OnShouldFilterAnimAsset() 수정
**파일:** `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.cpp`

- [ ] 문자열 비교 → 포인터 비교로 변경
- [ ] `Anim->Skeleton != TargetSkeleton` 조건 사용

**체크포인트:** AnimToPlay 필터링 테스트 성공

---

### Phase 6: 정리 및 초기화 ⏱️ 30분

#### Step 6.1: EditorEngine 통합
**파일:** `Mundi/Source/Runtime/Engine/GameFramework/EditorEngine.cpp`

- [ ] `#include "FbxManager.h"` 추가
- [ ] `Initialize()`에서 `FFbxManager::Preload()` 호출
- [ ] `Shutdown()`에서 `FFbxManager::Clear()` 호출

**체크포인트:** 엔진 시작/종료 시 메모리 누수 없음

---

## 파일별 변경 사항

### 1. `Mundi/Source/Editor/FbxManager.h` (신규)

```cpp
#pragma once
#include "String.h"
#include "UEContainer.h"

struct FSkeleton;
struct FSkeletalMesh;

/**
 * FFbxManager
 * FBX 애셋의 메모리 레벨 캐싱 관리
 * 패턴: FObjManager (정적 캐시 클래스)
 */
class FFbxManager
{
private:
    static TMap<FString, FSkeleton*> SkeletonMap;
    static TMap<FString, FSkeletalMesh*> SkeletalMeshMap;

public:
    /**
     * GetSkeleton - 캐시된 Skeleton 조회
     * @param FilePath - FBX 파일 경로
     * @return FSkeleton* 또는 nullptr
     */
    static FSkeleton* GetSkeleton(const FString& FilePath);

    /**
     * RegisterSkeleton - Skeleton을 캐시에 등록
     * @param FilePath - FBX 파일 경로
     * @param Skeleton - 등록할 Skeleton 포인터
     */
    static void RegisterSkeleton(const FString& FilePath, FSkeleton* Skeleton);

    /**
     * GetAllSkeletonNames - 모든 Skeleton 이름 반환 (UI용)
     * @return Skeleton 이름 목록
     */
    static TArray<FString> GetAllSkeletonNames();

    /**
     * LoadSkeletalMeshAsset - SkeletalMesh 로드 (캐시 우선)
     * @param FilePath - FBX 파일 경로
     * @return FSkeletalMesh* 또는 nullptr
     */
    static FSkeletalMesh* LoadSkeletalMeshAsset(const FString& FilePath);

    /**
     * Clear - 모든 캐시 정리
     */
    static void Clear();

    /**
     * Preload - 사전 로딩
     */
    static void Preload();
};
```

---

### 2. `Mundi/Source/Editor/FbxManager.cpp` (신규)

```cpp
#include "pch.h"
#include "FbxManager.h"
#include "FbxLoader.h"
#include "PathUtils.h"
#include "VertexData.h"
#include "GlobalConsole.h"

TMap<FString, FSkeleton*> FFbxManager::SkeletonMap;
TMap<FString, FSkeletalMesh*> FFbxManager::SkeletalMeshMap;

FSkeleton* FFbxManager::GetSkeleton(const FString& FilePath)
{
    FString NormalizedPath = NormalizePath(FilePath);

    if (FSkeleton** It = SkeletonMap.Find(NormalizedPath))
    {
        return *It;
    }

    return nullptr;
}

void FFbxManager::RegisterSkeleton(const FString& FilePath, FSkeleton* Skeleton)
{
    if (!Skeleton) return;

    FString NormalizedPath = NormalizePath(FilePath);

    if (SkeletonMap.Find(NormalizedPath) == nullptr)
    {
        SkeletonMap.Add(NormalizedPath, Skeleton);
        UE_LOG("FFbxManager: Registered Skeleton: %s", Skeleton->Name.c_str());
    }
}

TArray<FString> FFbxManager::GetAllSkeletonNames()
{
    TArray<FString> Names;
    for (auto& Pair : SkeletonMap)
    {
        if (Pair.second && !Pair.second->Name.empty())
        {
            Names.Add(Pair.second->Name);
        }
    }
    return Names;
}

FSkeletalMesh* FFbxManager::LoadSkeletalMeshAsset(const FString& FilePath)
{
    FString NormalizedPath = NormalizePath(FilePath);

    // 1. 메모리 캐시 확인
    if (FSkeletalMesh** It = SkeletalMeshMap.Find(NormalizedPath))
    {
        UE_LOG("FFbxManager: Cache hit for: %s", NormalizedPath.c_str());
        return *It;
    }

    // 2. UFbxLoader를 통해 로드 (디스크 캐시 + 파싱)
    FSkeletalMesh* MeshData = UFbxLoader::GetInstance().LoadFbxMeshAsset(NormalizedPath);

    if (!MeshData)
    {
        return nullptr;
    }

    // 3. 메모리 캐시에 등록
    SkeletalMeshMap.Add(NormalizedPath, MeshData);

    if (MeshData->Skeleton)
    {
        SkeletonMap.Add(NormalizedPath, MeshData->Skeleton);
    }

    UE_LOG("FFbxManager: Loaded and cached: %s", NormalizedPath.c_str());

    return MeshData;
}

void FFbxManager::Clear()
{
    // FSkeletalMesh가 FSkeleton을 소유하므로 MeshData만 삭제
    for (auto& Pair : SkeletalMeshMap)
    {
        delete Pair.second;
    }

    SkeletalMeshMap.Empty();
    SkeletonMap.Empty();

    UE_LOG("FFbxManager: Cleared all cached assets");
}

void FFbxManager::Preload()
{
    // UFbxLoader의 PreLoad 재사용
    UFbxLoader::PreLoad();
}
```

---

### 3. `Mundi/Source/Runtime/Core/Misc/VertexData.h` (수정)

#### 변경 전
```cpp
struct FSkeletalMesh
{
    FSkeleton Skeleton;  // 직접 포함
    // ...
};
```

#### 변경 후
```cpp
struct FSkeletalMesh
{
    FSkeleton* Skeleton = nullptr;  // 포인터로 변경

    // 하위 호환성 헬퍼
    const FSkeleton* GetSkeleton() const { return Skeleton; }
    FSkeleton* GetSkeleton() { return Skeleton; }

    // ... 기존 필드들 ...

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMesh& Data)
    {
        if (Ar.IsSaving())
        {
            // Skeleton 이름 저장
            FString SkeletonName = Data.Skeleton ? Data.Skeleton->Name : "";
            Serialization::WriteString(Ar, SkeletonName);
            // ... 나머지 저장 ...
        }
        else if (Ar.IsLoading())
        {
            // Skeleton 이름 로드 (포인터는 FbxLoader에서 설정)
            FString SkeletonName;
            Serialization::ReadString(Ar, SkeletonName);
            // ... 나머지 로드 ...
        }
        return Ar;
    }
};
```

---

### 4. `Mundi/Source/Runtime/AssetManagement/SkeletalMesh.h` (수정)

#### 추가 사항
```cpp
const FSkeleton* GetSkeleton() const
{
    return Data ? Data->Skeleton : nullptr;  // &Data->Skeleton 제거
}

void SetSkeleton(FSkeleton* NewSkeleton)
{
    if (Data)
        Data->Skeleton = NewSkeleton;
}

// GetBoneCount() 수정
uint32 GetBoneCount() const
{
    return (Data && Data->Skeleton) ? Data->Skeleton->Bones.Num() : 0;
}
```

---

### 5. `Mundi/Plugins/Fbx/FbxLoader.cpp` (수정)

#### LoadFbxMeshAsset() 수정

```cpp
#include "FbxManager.h"

FSkeletalMesh* UFbxLoader::LoadFbxMeshAsset(const FString& InFilePath)
{
    // ... FBX 파싱 ...

    // Skeleton 이름 추출
    FSkeleton ParsedSkeleton;
    ParsedSkeleton.Name = ExtractSkeletonName(InFilePath);
    // ... Bones 파싱 ...

    // FFbxManager에 Skeleton 등록 (중복 제거)
    FSkeleton* ManagedSkeleton = FFbxManager::RegisterSkeleton(InFilePath, &ParsedSkeleton);

    // FSkeletalMesh 생성
    FSkeletalMesh* Data = new FSkeletalMesh();
    Data->Skeleton = ManagedSkeleton;  // 포인터 저장
    Data->PathFileName = InFilePath;
    // ...

    return Data;
}

// 헬퍼 함수 추가
FString UFbxLoader::ExtractSkeletonName(const FString& FilePath)
{
    std::filesystem::path Path(FilePath);
    FString FileName = Path.stem().string();
    return FileName + "_Skeleton";
}
```

---

### 6. `Mundi/Source/Slate/Widgets/PropertyRenderer.cpp` (수정)

#### 제거할 코드 (라인 592-662)
```cpp
// SkeletonID property는 드롭다운으로 렌더링
if (FString(Prop.Name) == "SkeletonID")
{
    // ... 전체 블록 제거 ...
}
```

#### 수정 후
```cpp
bool UPropertyRenderer::RenderStringProperty(const FProperty& Prop, void* Instance)
{
    FString* Value = Prop.GetValuePtr<FString>(Instance);

    // 일반 FString property는 텍스트 입력으로 렌더링
    char Buffer[256];
    strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
    Buffer[sizeof(Buffer) - 1] = '\0';

    if (ImGui::InputText(Prop.Name, Buffer, sizeof(Buffer)))
    {
        *Value = Buffer;
        return true;
    }

    return false;
}
```

---

### 7. `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.h` (수정)

```cpp
private:
    bool OnShouldFilterAnimAsset(const FString& AssetPath);

    // 새 함수 추가
    void RenderSkeletonSelector();
    void OnSkeletonChanged(FSkeleton* NewSkeleton);

private:
    FSkeleton* TargetSkeleton = nullptr;  // FString에서 포인터로 변경
    USkeletalMeshComponent* CurrentComponent = nullptr;  // 추가
};
```

---

### 8. `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.cpp` (수정)

#### CustomizeDetails() 수정

```cpp
#include "FbxManager.h"

void FSkeletalMeshComponentCustomization::CustomizeDetails(UObject* Object)
{
    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Object);
    if (!SkelComp)
    {
        UE_LOG("FSkeletalMeshComponentCustomization: Object is not USkeletalMeshComponent");
        return;
    }

    CurrentComponent = SkelComp;

    // Skeleton 포인터 추출
    TargetSkeleton = nullptr;
    if (SkelComp->GetSkeletalMesh())
    {
        TargetSkeleton = const_cast<FSkeleton*>(
            SkelComp->GetSkeletalMesh()->GetSkeleton()
        );
    }

    // AnimToPlay 유효성 검사 (포인터 비교)
    if (SkelComp->AnimToPlay)
    {
        if (!TargetSkeleton || SkelComp->AnimToPlay->Skeleton != TargetSkeleton)
        {
            SkelComp->AnimToPlay = nullptr;
            UE_LOG("AnimToPlay cleared: incompatible Skeleton");
        }
    }

    // === Skeleton 선택 UI 렌더링 ===
    RenderSkeletonSelector();

    // 필터 델리게이트 (포인터 비교)
    FOnShouldFilterAsset AssetFilterDelegate;
    AssetFilterDelegate.Bind([this](const FString& AssetPath) -> bool {
        return OnShouldFilterAnimAsset(AssetPath);
    });

    FOnShouldFilterProperty PropertyFilterDelegate;
    PropertyFilterDelegate.Bind([SkelComp](const FString& PropertyName) -> bool {
        if (PropertyName == "AnimToPlay")
        {
            return SkelComp->AnimationMode != EAnimationMode::AnimationSingleNode;
        }
        return false;
    });

    UPropertyRenderer::RenderPropertiesWithCustomization(
        SkelComp, AssetFilterDelegate, PropertyFilterDelegate
    );
}
```

#### RenderSkeletonSelector() 구현 (신규)

```cpp
void FSkeletalMeshComponentCustomization::RenderSkeletonSelector()
{
    if (!CurrentComponent) return;

    ImGui::Separator();
    ImGui::Text("[Skeleton Management]");

    // 현재 Skeleton 이름
    FString CurrentSkeletonName = TargetSkeleton ? TargetSkeleton->Name : "None";

    // FFbxManager에서 Skeleton 목록 가져오기
    TArray<FString> AllSkeletonNames = FFbxManager::GetAllSkeletonNames();

    // ComboBox 아이템 배열
    TArray<const char*> Items;
    Items.Add("None");
    int CurrentIdx = 0;

    for (int i = 0; i < AllSkeletonNames.Num(); ++i)
    {
        Items.Add(AllSkeletonNames[i].c_str());
        if (AllSkeletonNames[i] == CurrentSkeletonName)
        {
            CurrentIdx = i + 1;
        }
    }

    // 드롭다운 렌더링
    ImGui::SetNextItemWidth(240);
    if (ImGui::Combo("Change Skeleton", &CurrentIdx, Items.data(), Items.size()))
    {
        if (CurrentIdx == 0)
        {
            OnSkeletonChanged(nullptr);
        }
        else
        {
            FString SelectedName = AllSkeletonNames[CurrentIdx - 1];
            FSkeleton* NewSkeleton = FFbxManager::GetSkeleton(SelectedName);
            OnSkeletonChanged(NewSkeleton);
        }
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Change the Skeleton for both SkeletalMesh and AnimToPlay");
        ImGui::EndTooltip();
    }
}
```

#### OnSkeletonChanged() 구현 (신규)

```cpp
void FSkeletalMeshComponentCustomization::OnSkeletonChanged(FSkeleton* NewSkeleton)
{
    if (!CurrentComponent) return;

    USkeletalMesh* Mesh = CurrentComponent->GetSkeletalMesh();
    UAnimSequence* Anim = CurrentComponent->AnimToPlay;

    // 1. SkeletalMesh의 Skeleton 교체
    if (Mesh)
    {
        Mesh->SetSkeleton(NewSkeleton);
        Mesh->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";
        UE_LOG("Changed SkeletalMesh Skeleton to: %s",
               NewSkeleton ? NewSkeleton->Name.c_str() : "None");
    }

    // 2. AnimToPlay의 Skeleton 교체
    if (Anim)
    {
        Anim->Skeleton = NewSkeleton;
        Anim->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";
        UE_LOG("Changed Animation Skeleton to: %s",
               NewSkeleton ? NewSkeleton->Name.c_str() : "None");
    }

    // 3. TargetSkeleton 업데이트
    TargetSkeleton = NewSkeleton;

    // 4. AnimToPlay 호환성 확인
    if (Anim && NewSkeleton && Anim->Skeleton != NewSkeleton)
    {
        CurrentComponent->AnimToPlay = nullptr;
        UE_LOG("AnimToPlay cleared: incompatible with new Skeleton");
    }
}
```

#### OnShouldFilterAnimAsset() 수정

```cpp
bool FSkeletalMeshComponentCustomization::OnShouldFilterAnimAsset(const FString& AssetPath)
{
    if (AssetPath.empty()) return false;
    if (!TargetSkeleton) return true;

    UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AssetPath);
    if (!Anim) return true;

    // 포인터 비교
    return (Anim->Skeleton != TargetSkeleton);
}
```

---

### 9. `Mundi/Source/Runtime/Engine/GameFramework/EditorEngine.cpp` (수정)

```cpp
#include "FbxManager.h"

void UEditorEngine::Initialize()
{
    // ... 기존 코드 ...

    // FbxManager 초기화
    FFbxManager::Preload();

    // ... 기존 코드 ...
}

void UEditorEngine::Shutdown()
{
    // ... 기존 코드 ...

    // FbxManager 정리
    FFbxManager::Clear();

    // ... 기존 코드 ...
}
```

---

## 테스트 계획

### Unit Tests

#### 1. FFbxManager 캐싱 테스트
```cpp
// Test 1: Skeleton 등록 및 조회
FSkeleton skeleton;
skeleton.Name = "TestSkeleton";
FFbxManager::RegisterSkeleton("test.fbx", &skeleton);

FSkeleton* retrieved = FFbxManager::GetSkeleton("test.fbx");
assert(retrieved == &skeleton);
```

#### 2. 중복 제거 테스트
```cpp
// Test 2: 같은 경로에 다시 등록 시 기존 것 유지
FSkeleton skeleton1, skeleton2;
FFbxManager::RegisterSkeleton("test.fbx", &skeleton1);
FFbxManager::RegisterSkeleton("test.fbx", &skeleton2);

FSkeleton* retrieved = FFbxManager::GetSkeleton("test.fbx");
assert(retrieved == &skeleton1);  // 첫 번째 것 유지
```

### Integration Tests

#### 1. FBX 로딩 테스트
```
1. Character.fbx 로드
2. FFbxManager::GetSkeleton("Character.fbx") 호출
3. 반환된 포인터 != nullptr 확인
4. 같은 파일 다시 로드
5. 같은 포인터 반환되는지 확인 (캐시 히트)
```

#### 2. Skeleton 교체 테스트
```
1. SkeletalMeshComponent 생성
2. Character.fbx 메시 설정
3. Hero.fbx 애니메이션 설정
4. Property Window 열기
5. Skeleton 드롭다운에서 "Character_Skeleton" 선택
6. AnimToPlay 드롭다운에 Character와 Hero 애니메이션 모두 표시되는지 확인
```

### Performance Tests

#### 1. 메모리 사용량 테스트
```
Before: 100GB+ (전체 리소스 스캔)
After: < 1GB (Skeleton 포인터만 캐싱)

측정 방법:
- Task Manager에서 메모리 사용량 확인
- Property Window 열었을 때 메모리 급증 없는지 확인
```

#### 2. 로딩 속도 테스트
```
Before: 첫 로딩 느림 + 캐시 없음
After: 첫 로딩 느림 + 캐시 히트 시 즉시 반환

측정 방법:
- 같은 FBX 파일 10번 로드
- 2번째부터 로딩 시간 < 1ms 확인
```

---

## 참고 자료

### 관련 파일

| 파일 경로 | 역할 |
|----------|------|
| `Mundi/Source/Editor/ObjManager.h` | 참고 패턴 (FObjManager) |
| `Mundi/Plugins/Fbx/FbxLoader.h` | 기존 FBX 로더 |
| `Mundi/Source/Runtime/Core/Misc/VertexData.h` | FSkeleton, FSkeletalMesh 정의 |
| `Mundi/Source/Slate/PropertyCustomization/SkeletalMeshComponentCustomization.h` | UI 커스터마이제이션 |

### 핵심 개념

#### 1. Skeleton 포인터 공유
```
Character.fbx 로드
└─> FSkeleton* pSkeleton = new FSkeleton("Character_Skeleton")
    ├─> FFbxManager::SkeletonMap["Character.fbx"] = pSkeleton
    └─> Character.SkeletalMesh->Data->Skeleton = pSkeleton

Hero.fbx 로드 (같은 Skeleton 이름)
└─> FSkeleton* pExisting = FFbxManager::GetSkeleton("Character.fbx")
    └─> Hero.SkeletalMesh->Data->Skeleton = pExisting

결과: Character와 Hero가 같은 FSkeleton 객체 공유
포인터 비교: pSkeleton == pExisting ✅
```

#### 2. 메모리 효율성
```
Before (SkeletonID 방식):
- 100개 메시 × 10MB = 1GB
- 전체 스캔 시 모든 메시 메모리 접근

After (FFbxManager 방식):
- 10개 Skeleton × 100KB = 1MB
- 드롭다운은 Skeleton 이름만 표시 (메모리 접근 최소)
```

---

## 체크리스트 요약

### Phase 1: FFbxManager 생성
- [ ] FbxManager.h 생성
- [ ] FbxManager.cpp 생성
- [ ] Mundi.vcxproj 업데이트
- [ ] 컴파일 테스트

### Phase 2: FSkeletalMesh 구조 수정
- [ ] VertexData.h 수정 (Skeleton → Skeleton*)
- [ ] USkeletalMesh::GetSkeleton() 수정
- [ ] SetSkeleton() 추가

### Phase 3: UFbxLoader 통합
- [ ] LoadFbxMeshAsset() 수정
- [ ] LoadAnimationAsset() 수정
- [ ] Skeleton 자동 등록

### Phase 4: PropertyRenderer 정리
- [ ] SkeletonID 드롭다운 제거
- [ ] CachedSkeletonIDs 제거

### Phase 5: UI 통합
- [ ] SkeletalMeshComponentCustomization.h 수정
- [ ] CustomizeDetails() 수정
- [ ] RenderSkeletonSelector() 구현
- [ ] OnSkeletonChanged() 구현
- [ ] OnShouldFilterAnimAsset() 수정 (포인터 비교)

### Phase 6: 정리
- [ ] EditorEngine 초기화/정리
- [ ] 메모리 누수 확인

### 테스트
- [ ] FBX 로딩 테스트
- [ ] Skeleton 교체 테스트
- [ ] AnimToPlay 필터링 테스트
- [ ] 메모리 사용량 테스트

---

## 완료 조건

✅ **기능 완료:**
- FFbxManager가 Skeleton을 중앙 관리
- Property Window에 Skeleton 드롭다운 표시
- Skeleton 교체 시 Mesh + Anim 동시 적용
- AnimToPlay 필터링이 포인터 비교로 동작

✅ **성능 완료:**
- 메모리 사용량 < 1GB (100GB+ 문제 해결)
- 캐시 히트 시 로딩 < 1ms

✅ **품질 완료:**
- 컴파일 에러 없음
- 메모리 누수 없음
- 모든 테스트 통과

---

**작성일:** 2025-11-18
**작성자:** Claude Code
**버전:** 1.0
