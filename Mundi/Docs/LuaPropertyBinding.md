# Lua Property Binding Guide

## Overview

이 문서는 C++ 클래스의 프로퍼티를 Lua 스크립트에 노출하는 방법을 설명합니다.

**최종 업데이트**: 2025-11-14
**작성자**: Claude & User

### 변경 이력

- **2025-11-14**:
  - ✅ UPROPERTY(LuaReadWrite) 메타데이터 파이프라인 구현 완료
  - ✅ 재귀 UObject 프로퍼티 접근 지원
  - ✅ uint32, uint64, int64, double 타입 바인딩 추가
  - ✅ UStaticMesh를 리플렉션 시스템으로 전환 (예제)
  - ✅ TArray<Primitive> 완전 지원 (int32, float, FString, FVector 등)
  - ✅ TArray<UObject*> 완전 지원 (모든 UObject 파생 타입)
  - ✅ TMap<K, V> 완전 지원 (모든 primitive/struct/UObject* 조합)
  - ✅ LuaArrayProxy 및 LuaMapProxy SRP 설계 구현
  - ✅ Documentation 작성

## Quick Start

### 1. 기본 사용법

C++ 헤더 파일에서 `UPROPERTY(LuaReadWrite, ...)`를 추가하면 자동으로 Lua에 노출됩니다:

```cpp
// MyComponent.h
#include "UMyComponent.generated.h"

UCLASS(DisplayName="My Component", Description="Custom component")
class UMyComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    // Lua에서 읽기/쓰기 가능한 프로퍼티
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Settings")
    float Speed = 100.0f;

    UPROPERTY(LuaReadWrite, Category="Settings")
    bool bIsEnabled = true;

    // UObject 포인터도 지원 (재귀 접근 가능)
    UPROPERTY(LuaReadWrite, Category="References")
    UStaticMesh* MyMesh = nullptr;
};
```

```cpp
// MyComponent.cpp
#include "pch.h"
#include "MyComponent.h"

// IMPLEMENT_CLASS는 더 이상 필요 없음 (코드 생성기가 자동 생성)
```

### 2. Lua에서 사용

```lua
-- Test.lua
function BeginPlay()
    local comp = GetComponent(Obj, "UMyComponent")

    -- 프로퍼티 읽기
    print("Speed: " .. comp.Speed)  -- 100.0
    print("Enabled: " .. tostring(comp.bIsEnabled))  -- true

    -- 프로퍼티 쓰기
    comp.Speed = 200.0
    comp.bIsEnabled = false

    -- UObject 재귀 접근
    if comp.MyMesh then
        print("Vertex count: " .. comp.MyMesh.TestVertexCount)
    end
end
```

---

## 지원되는 타입

### Primitive 타입
- `bool` - Boolean values
- `int32`, `int` - 32-bit signed integers
- `uint32`, `unsigned int` - 32-bit unsigned integers (Int32로 매핑, Lua는 부호 구분 없음)
- `int64`, `long long` - 64-bit signed integers (Int32로 매핑, 정밀도 손실 가능)
- `uint64`, `unsigned long long` - 64-bit unsigned integers (Int32로 매핑, 정밀도 손실 가능)
- `float` - Single precision floating point
- `double` - Double precision floating point (Float로 매핑, Lua number는 기본 double)
- `FString` - String values
- `FName` - Name values (Lua에서는 string으로 반환)

### Struct 타입
- `FVector` - 3D vector (X, Y, Z)
- `FLinearColor` - RGBA color

### UObject 포인터 타입
- `UObject*` - 모든 UObject 파생 타입
- `UTexture*` - Texture 리소스
- `UStaticMesh*` - Static mesh 리소스
- `USkeletalMesh*` - Skeletal mesh 리소스
- `UMaterialInterface*` - Material 리소스
- `USound*` - Sound 리소스

### Container 타입

#### TArray (배열)
**Primitive 타입 배열**:
- `TArray<int32>` - 정수 배열
- `TArray<float>` - 실수 배열
- `TArray<FString>` - 문자열 배열
- `TArray<FVector>` - 벡터 배열
- `TArray<bool>`, `TArray<double>` 등 모든 primitive 타입

**UObject 포인터 배열**:
- `TArray<UObject*>` - 모든 UObject 배열
- `TArray<UTexture*>` - 텍스처 배열
- `TArray<UMaterialInterface*>` - 머티리얼 배열
- 기타 모든 UObject* 파생 타입

#### TMap (맵/딕셔너리)
**Primitive Key/Value**:
- `TMap<FString, int32>` - 문자열 → 정수
- `TMap<int32, float>` - 정수 → 실수
- `TMap<FString, FVector>` - 문자열 → 벡터

**UObject 포인터 Value**:
- `TMap<FString, UTexture*>` - 문자열 → 텍스처
- `TMap<int32, UMaterialInterface*>` - 정수 → 머티리얼
- 기타 모든 조합 가능

---

## 메타데이터 파이프라인

### 작동 방식

1. **헤더 파일**: `UPROPERTY(LuaReadWrite, ...)` 선언
2. **Python 파서**: `BuildTools/CodeGenerator/header_parser.py`가 메타데이터 파싱
3. **코드 생성**: `property_generator.py`가 `ADD_PROPERTY_METADATA()` 호출 생성
4. **C++ 매크로**: `ObjectMacros.h`의 `ADD_PROPERTY_METADATA`가 런타임에 메타데이터 등록
5. **Lua 바인딩**: `LuaObjectProxy.cpp`의 `BuildBoundClass()`가 메타데이터 확인 후 노출

### 생성된 코드 예시

```cpp
// UMyComponent.generated.cpp (자동 생성)
BEGIN_PROPERTIES(UMyComponent)
    MARK_AS_COMPONENT("My Component", "Custom component")
    ADD_PROPERTY(float, Speed, "Settings", true)
    ADD_PROPERTY_METADATA(Speed, "LuaReadWrite", "true")  // ← 이 부분이 핵심!
    ADD_PROPERTY(bool, bIsEnabled, "Settings", true)
    ADD_PROPERTY_METADATA(bIsEnabled, "LuaReadWrite", "true")
END_PROPERTIES()
```

---

## ResourceBase 파생 클래스를 리플렉션으로 전환하기

### Before: DECLARE_CLASS 사용

```cpp
// OldResource.h
#pragma once
#include "ResourceBase.h"

class UOldResource : public UResourceBase
{
public:
    DECLARE_CLASS(UOldResource, UResourceBase)

    UOldResource() = default;
    virtual ~UOldResource() override;

    float SomeValue = 1.0f;  // Lua에 노출 불가
};
```

```cpp
// OldResource.cpp
#include "pch.h"
#include "OldResource.h"

IMPLEMENT_CLASS(UOldResource)  // 수동 등록 필요
```

### After: GENERATED_REFLECTION_BODY 사용

```cpp
// NewResource.h
#pragma once
#include "ResourceBase.h"
#include "UNewResource.generated.h"  // ← 1. 생성될 헤더 include

UCLASS(DisplayName="New Resource", Description="Example resource")  // ← 2. UCLASS 추가
class UNewResource : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()  // ← 3. DECLARE_CLASS를 이것으로 교체

    UNewResource() = default;
    virtual ~UNewResource() override;

    // ← 4. LuaReadWrite 추가하면 Lua에 노출됨
    UPROPERTY(LuaReadWrite, Category="Settings")
    float SomeValue = 1.0f;
};
```

```cpp
// NewResource.cpp
#include "pch.h"
#include "NewResource.h"

// IMPLEMENT_CLASS 제거 - 코드 생성기가 자동 생성
```

### 변경 체크리스트

1. ✅ 헤더 파일 최상단에 `#include "UClassName.generated.h"` 추가
2. ✅ 클래스 선언 전에 `UCLASS(...)` 매크로 추가
3. ✅ `DECLARE_CLASS(...)` → `GENERATED_REFLECTION_BODY()` 교체
4. ✅ CPP 파일에서 `IMPLEMENT_CLASS(...)` 제거
5. ✅ Lua에 노출하려는 프로퍼티에 `UPROPERTY(LuaReadWrite, ...)` 추가
6. ✅ 빌드 (코드 생성기가 자동 실행됨)

---

## 예제: UStaticMesh 변환 사례

### 변경 전후 비교

**Before** (`StaticMesh.h`):
```cpp
#pragma once
#include "ResourceBase.h"

class UStaticMesh : public UResourceBase
{
public:
    DECLARE_CLASS(UStaticMesh, UResourceBase)

    UStaticMesh() = default;
    virtual ~UStaticMesh() override;

    // 내부 변수 - Lua에서 접근 불가
    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
};
```

**After** (`StaticMesh.h`):
```cpp
#pragma once
#include "ResourceBase.h"
#include "UStaticMesh.generated.h"  // 추가

UCLASS(DisplayName="스태틱 메시", Description="정적 메시 에셋입니다")  // 추가
class UStaticMesh : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()  // 교체

    UStaticMesh() = default;
    virtual ~UStaticMesh() override;

    // Lua에서 접근 가능하도록 변경
    UPROPERTY(LuaReadWrite, Category="Mesh Info")
    int32 TestVertexCount = 0;

    UPROPERTY(LuaReadWrite, Category="Mesh Info")
    int32 TestIndexCount = 0;
};
```

**Lua에서 사용**:
```lua
local staticMeshComp = GetComponent(Obj, "UStaticMeshComponent")
local mesh = staticMeshComp.StaticMesh

-- 재귀 접근 성공!
print("Vertices: " .. mesh.TestVertexCount)  -- 1827
print("Indices: " .. mesh.TestIndexCount)    -- 5478
```

---

## 타입 바인딩 추가하기

### 현재 지원되는 타입 (ObjectMacros.h)

```cpp
template<typename T>
struct TPropertyTypeTraits
{
    static constexpr EPropertyType GetType()
    {
        if constexpr (std::is_same_v<T, bool>)
            return EPropertyType::Bool;
        else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int>)
            return EPropertyType::Int32;
        else if constexpr (std::is_same_v<T, float>)
            return EPropertyType::Float;
        else if constexpr (std::is_same_v<T, FVector>)
            return EPropertyType::FVector;
        else if constexpr (std::is_same_v<T, FLinearColor>)
            return EPropertyType::FLinearColor;
        else if constexpr (std::is_same_v<T, FString>)
            return EPropertyType::FString;
        else if constexpr (std::is_same_v<T, FName>)
            return EPropertyType::FName;
        else if constexpr (std::is_pointer_v<T>)
            return EPropertyType::ObjectPtr;
        else
            return EPropertyType::Struct;
    }
};
```

### 추가 지원 타입 (2025-11-14 업데이트)

**uint32/uint64/int64/double 지원 추가됨!**

```cpp
// 이제 모두 사용 가능!
UPROPERTY(LuaReadWrite, Category="Stats")
uint32 EntityCount = 0;  // ✅ 지원됨

UPROPERTY(LuaReadWrite, Category="Stats")
uint64 TotalScore = 0;   // ✅ 지원됨 (큰 값은 정밀도 손실 가능)

UPROPERTY(LuaReadWrite, Category="Physics")
double Precision = 0.0;  // ✅ 지원됨
```

**주의사항**:
- Lua number는 기본적으로 `double` (IEEE 754)
- `int64`/`uint64`는 큰 값(> 2^53)에서 정밀도 손실 가능
- 부호(signed/unsigned) 정보는 Lua에서 손실됨 (음수 체크는 C++ 레이어에서 수행)

---

## 주의사항

### 1. EditAnywhere vs LuaReadWrite

- **EditAnywhere**: 에디터 UI에서 편집 가능 (DetailWidget)
- **LuaReadWrite**: Lua 스크립트에서 접근 가능
- 두 메타데이터는 **독립적**이며, 필요에 따라 조합 가능:

```cpp
UPROPERTY(LuaReadWrite, EditAnywhere, Category="Settings")
float Speed = 100.0f;  // 에디터 + Lua 모두 접근 가능

UPROPERTY(LuaReadWrite, Category="Internal")
int32 InternalCounter = 0;  // Lua만 접근 가능 (에디터 숨김)

UPROPERTY(EditAnywhere, Category="Display")
FString DisplayName;  // 에디터만 접근 가능 (Lua 숨김)
```

### 2. Abstract 클래스

UResourceBase처럼 추상 클래스는 UCLASS에 Abstract 플래그 필요:

```cpp
UCLASS(Abstract, DisplayName="리소스 베이스", Description="모든 리소스의 기본 클래스입니다")
class UResourceBase : public UObject
{
    GENERATED_REFLECTION_BODY()
    // ...
};
```

### 3. 정수 타입 선택 가이드

모든 정수 타입이 지원되지만, 용도에 맞게 선택하세요:
- **uint32/int32**: 일반적인 게임 로직 (카운터, ID, 플래그)
- **uint64/int64**: 큰 값이 필요한 경우 (정밀도 주의)
- **Lua 권장**: Lua number는 부호가 없으므로, C++에서 음수 검증 수행

### 4. 순환 Include 방지

ObjectMacros.h는 UCLASS 매크로를 최상단에 정의하여 순환 include를 방지합니다.
새로운 타입을 추가할 때는 forward declaration 사용 권장.

---

## 디버깅

### Lua 바인딩 로그 확인

Visual Studio Output 창에서 다음과 같은 로그 확인:

```
[LuaObjectProxy] Bound class: UMyComponent (5/10 properties exposed to Lua)
[LuaObjectProxy] Bound class: UStaticMesh (2/2 properties exposed to Lua)
```

- **첫 번째 숫자**: LuaReadWrite가 있는 프로퍼티 개수
- **두 번째 숫자**: 전체 프로퍼티 개수

### 메타데이터 확인

생성된 `.generated.cpp` 파일에서 `ADD_PROPERTY_METADATA` 확인:

```cpp
// Generated\UMyComponent.generated.cpp
BEGIN_PROPERTIES(UMyComponent)
    ADD_PROPERTY(float, Speed, "Settings", true)
    ADD_PROPERTY_METADATA(Speed, "LuaReadWrite", "true")  // ← 이 줄이 있어야 함
END_PROPERTIES()
```

### Lua에서 nil 반환 시

1. 메타데이터가 제대로 생성되었는지 확인
2. 프로퍼티 타입이 TPropertyTypeTraits에서 지원되는지 확인
3. LuaObjectProxy::Index()에서 해당 타입의 case가 있는지 확인

---

## 참고 파일

- **코드 생성기**: `BuildTools/CodeGenerator/`
  - `header_parser.py` - UPROPERTY 파싱
  - `property_generator.py` - .generated.cpp 생성
- **C++ 매크로**: `Source/Runtime/Core/Object/ObjectMacros.h`
- **Lua 바인딩**: `Source/Runtime/Engine/Scripting/LuaObjectProxy.cpp`
- **테스트**: `Data/Scripts/Test.lua`

---

## Container 사용 가이드

### TArray 사용법

#### C++ 선언

```cpp
UCLASS()
class UInventoryComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    // Primitive 타입 배열
    UPROPERTY(LuaReadWrite, Category="Inventory")
    TArray<int32> ItemIDs;

    UPROPERTY(LuaReadWrite, Category="Inventory")
    TArray<FString> ItemNames;

    UPROPERTY(LuaReadWrite, Category="Inventory")
    TArray<FVector> ItemPositions;

    // UObject 포인터 배열
    UPROPERTY(LuaReadWrite, Category="Inventory")
    TArray<UTexture*> ItemIcons;
};
```

#### Lua 사용

```lua
function BeginPlay()
    local inv = GetComponent(Obj, "UInventoryComponent")

    -- 배열 길이 확인 (# 연산자)
    print("Item count: " .. #inv.ItemIDs)

    -- 배열 읽기 (1-based 인덱스)
    for i = 1, #inv.ItemNames do
        print("Item " .. i .. ": " .. inv.ItemNames[i])
    end

    -- 배열 쓰기
    inv.ItemIDs[1] = 100         -- 첫 번째 요소 수정
    inv.ItemNames[1] = "Sword"   -- 문자열 수정

    -- 배열에 새 요소 추가
    local newIndex = #inv.ItemIDs + 1
    inv.ItemIDs[newIndex] = 200
    inv.ItemNames[newIndex] = "Shield"

    -- 벡터 배열
    inv.ItemPositions[1] = Vector(10.0, 20.0, 30.0)
    local pos = inv.ItemPositions[1]
    print("Position: " .. pos.X .. ", " .. pos.Y .. ", " .. pos.Z)
end
```

### TMap 사용법

#### C++ 선언

```cpp
UCLASS()
class UGameStateComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    // Primitive 타입 맵
    UPROPERTY(LuaReadWrite, Category="Game State")
    TMap<FString, int32> PlayerScores;

    UPROPERTY(LuaReadWrite, Category="Game State")
    TMap<int32, float> LevelDifficulties;

    UPROPERTY(LuaReadWrite, Category="Game State")
    TMap<FString, FVector> SpawnPoints;

    // UObject 포인터 맵
    UPROPERTY(LuaReadWrite, Category="Resources")
    TMap<FString, UTexture*> TextureCache;
};
```

#### Lua 사용

```lua
function BeginPlay()
    local gameState = GetComponent(Obj, "UGameStateComponent")

    -- 맵에 값 추가/수정
    gameState.PlayerScores["Player1"] = 100
    gameState.PlayerScores["Player2"] = 200
    gameState.PlayerScores["Player3"] = 150

    -- 맵에서 값 읽기
    print("Player1 score: " .. gameState.PlayerScores["Player1"])

    -- 정수 키 맵
    gameState.LevelDifficulties[1] = 0.5
    gameState.LevelDifficulties[2] = 0.8
    gameState.LevelDifficulties[10] = 1.5

    print("Level 1 difficulty: " .. gameState.LevelDifficulties[1])

    -- 벡터 값 맵
    gameState.SpawnPoints["Start"] = Vector(0, 0, 0)
    gameState.SpawnPoints["Boss"] = Vector(100, 200, 50)

    local spawnPos = gameState.SpawnPoints["Start"]
    print("Spawn at: " .. spawnPos.X .. ", " .. spawnPos.Y .. ", " .. spawnPos.Z)

    -- 키 삭제 (nil 할당)
    gameState.PlayerScores["Player2"] = nil
    print("Player2 score after deletion: " .. tostring(gameState.PlayerScores["Player2"]))  -- nil
end
```

### Container 주의사항

#### 1. Lua는 1-based 인덱싱
```lua
local arr = comp.MyArray
-- arr[1]  ← 첫 번째 요소 (C++의 [0])
-- arr[2]  ← 두 번째 요소 (C++의 [1])
```

#### 2. 배열 길이는 # 연산자
```lua
local length = #comp.MyArray
for i = 1, length do
    print(comp.MyArray[i])
end
```

#### 3. Map 키 삭제는 nil 할당
```lua
comp.MyMap["KeyToDelete"] = nil  -- 키 삭제
```

#### 4. UObject* 컨테이너는 nil 체크 필요
```lua
local textures = comp.TextureArray
if textures[1] ~= nil then
    -- 텍스처 사용
end
```

#### 5. 성능 고려사항
- 컨테이너 접근은 프록시를 통하므로 반복 접근 시 로컬 변수에 캐싱 권장:
```lua
-- Bad: 매번 프로퍼티 접근
for i = 1, 1000 do
    comp.MyArray[i] = i
end

-- Good: 로컬 변수에 캐싱
local arr = comp.MyArray
for i = 1, 1000 do
    arr[i] = i
end
```

---

## 예제 코드 전체

### C++ Component

```cpp
// MyGameComponent.h
#pragma once
#include "ActorComponent.h"
#include "UMyGameComponent.generated.h"

UCLASS(DisplayName="Game Component", Description="Example game logic component")
class UMyGameComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    // 기본 타입
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Gameplay")
    float Health = 100.0f;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Gameplay")
    int32 Score = 0;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Gameplay")
    bool bIsAlive = true;

    // UObject 참조
    UPROPERTY(LuaReadWrite, Category="References")
    UStaticMesh* WeaponMesh = nullptr;

    // Vector
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
    FVector Velocity = FVector(0, 0, 0);
};
```

### Lua Script

```lua
-- GameLogic.lua
function BeginPlay()
    local gameComp = GetComponent(Obj, "UMyGameComponent")

    -- 초기 상태 확인
    print("Health: " .. gameComp.Health)
    print("Score: " .. gameComp.Score)
    print("Alive: " .. tostring(gameComp.bIsAlive))

    -- 벡터 접근
    local vel = gameComp.Velocity
    print("Velocity: " .. vel.X .. ", " .. vel.Y .. ", " .. vel.Z)
end

function Tick(dt)
    local gameComp = GetComponent(Obj, "UMyGameComponent")

    -- 게임 로직
    if gameComp.bIsAlive then
        gameComp.Health = gameComp.Health - dt * 10

        if gameComp.Health <= 0 then
            gameComp.bIsAlive = false
            print("Game Over! Final Score: " .. gameComp.Score)
        end
    end
end

function OnHit(damage)
    local gameComp = GetComponent(Obj, "UMyGameComponent")
    gameComp.Health = gameComp.Health - damage
    gameComp.Score = gameComp.Score + 10
end
```

---

## 결론

이 시스템을 사용하면:
1. ✅ C++ 프로퍼티를 Lua에 쉽게 노출
2. ✅ 타입 안전성 보장 (컴파일 타임 체크)
3. ✅ UObject 재귀 접근 지원
4. ✅ **TArray 완전 지원** (primitive, struct, UObject*)
5. ✅ **TMap 완전 지원** (모든 키/값 조합)
6. ✅ 코드 생성 자동화 (빌드 시 자동 실행)
7. ✅ 에디터 UI와 Lua 바인딩을 독립적으로 제어
8. ✅ SRP 설계 (LuaArrayProxy, LuaMapProxy 분리)

필요한 클래스만 선택적으로 리플렉션 시스템으로 전환하면 됩니다!

## 참고 구현 파일

**Container 관련 파일**:
- `LuaArrayProxy.h/cpp` - TArray Lua 바인딩 전담
- `LuaMapProxy.h/cpp` - TMap Lua 바인딩 전담
- `LuaObjectProxyHelpers.h` - 공통 유틸리티 (타입 검증, UObject 매핑)
- `LuaObjectProxy.cpp` - 메인 프록시 (Container 프록시 생성)

**코드 생성 파일**:
- `BuildTools/CodeGenerator/generate.py` - TArray/TMap 타입 파싱
- `BuildTools/CodeGenerator/property_generator.py` - Container 프로퍼티 생성
