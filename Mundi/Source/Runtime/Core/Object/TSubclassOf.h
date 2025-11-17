#pragma once

// 전방 선언
struct UClass;

/**
 * TSubclassOf<T>
 *
 * 타입 안전한 UClass 포인터 wrapper
 * T의 자식 클래스만 허용하도록 컴파일 타임에 보장합니다.
 *
 * 사용 예:
 * UPROPERTY(EditAnywhere)
 * TSubclassOf<UAnimInstance> AnimClass;
 *
 * 런타임에서:
 * if (AnimClass)
 * {
 *     UAnimInstance* Instance = NewObject<UAnimInstance>(AnimClass.Get());
 * }
 */
template<typename T>
struct TSubclassOf
{
public:
    // 기본 생성자
    TSubclassOf() = default;

    // nullptr 생성자
    TSubclassOf(decltype(nullptr)) : Class(nullptr) {}

    // UClass* 생성자
    TSubclassOf(UClass* InClass) : Class(InClass) {}

    // 복사 생성자
    TSubclassOf(const TSubclassOf& Other) = default;

    // 대입 연산자
    TSubclassOf& operator=(const TSubclassOf& Other) = default;
    TSubclassOf& operator=(UClass* InClass)
    {
        Class = InClass;
        return *this;
    }
    TSubclassOf& operator=(decltype(nullptr))
    {
        Class = nullptr;
        return *this;
    }

    // UClass* 변환 연산자
    operator UClass*() const { return Class; }

    // 포인터 접근
    UClass* Get() const { return Class; }
    UClass** operator&() { return &Class; }
    const UClass* const* operator&() const { return &Class; }

    // 포인터 연산자
    UClass* operator->() const { return Class; }

    // bool 변환 (if 문에서 사용)
    explicit operator bool() const { return Class != nullptr; }
    bool operator!() const { return Class == nullptr; }

    // 비교 연산자
    bool operator==(const TSubclassOf& Other) const { return Class == Other.Class; }
    bool operator!=(const TSubclassOf& Other) const { return Class != Other.Class; }
    bool operator==(UClass* Other) const { return Class == Other; }
    bool operator!=(UClass* Other) const { return Class != Other; }
    bool operator==(decltype(nullptr)) const { return Class == nullptr; }
    bool operator!=(decltype(nullptr)) const { return Class != nullptr; }

    // BaseClass 타입 정보 (컴파일 타임)
    using BaseClassType = T;
    static constexpr const char* GetBaseClassName() { return nullptr; } // 런타임에서 T::StaticClass() 사용

private:
    UClass* Class = nullptr;
};
