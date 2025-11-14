#include "pch.h"
#include "LuaArrayProxy.h"
#include "LuaObjectProxy.h"
#include "LuaObjectProxyHelpers.h"
#include "ObjectFactory.h"

// External function from LuaManager.cpp
extern sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class);

// ===== Lua Metamethods =====

sol::object LuaArrayProxy::Index(sol::this_state LuaState, LuaArrayProxy& Self, int Index)
{
    if (!Self.Instance || !Self.Property) return sol::nil;
    if (Self.Property->Type != EPropertyType::Array) return sol::nil;

    sol::state_view LuaView(LuaState);

    // Convert 1-based Lua index to 0-based C++ index
    if (Index < 1) return sol::nil;
    size_t CppIndex = static_cast<size_t>(Index - 1);

    // Handle UObject pointer arrays
    if (IsObjectPointerType(Self.Property->InnerType))
    {
        TArray<UObject*>* ArrayPtr = Self.Property->GetValuePtr<TArray<UObject*>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;

        UObject* Element = (*ArrayPtr)[CppIndex];
        if (IsValidUObject(Element))
            return MakeCompProxy(LuaView, Element, Element->GetClass());
        return sol::nil;
    }

    // Handle primitive type arrays
    switch (Self.Property->InnerType)
    {
    case EPropertyType::Int32:
    {
        TArray<int32>* ArrayPtr = Self.Property->GetValuePtr<TArray<int32>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::Float:
    {
        TArray<float>* ArrayPtr = Self.Property->GetValuePtr<TArray<float>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::Bool:
    {
        TArray<bool>* ArrayPtr = Self.Property->GetValuePtr<TArray<bool>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::FString:
    {
        TArray<FString>* ArrayPtr = Self.Property->GetValuePtr<TArray<FString>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::FVector:
    {
        TArray<FVector>* ArrayPtr = Self.Property->GetValuePtr<TArray<FVector>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::FLinearColor:
    {
        TArray<FLinearColor>* ArrayPtr = Self.Property->GetValuePtr<TArray<FLinearColor>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex]);
    }
    case EPropertyType::FName:
    {
        TArray<FName>* ArrayPtr = Self.Property->GetValuePtr<TArray<FName>>(Self.Instance);
        if (!ArrayPtr || CppIndex >= ArrayPtr->size()) return sol::nil;
        return sol::make_object(LuaView, (*ArrayPtr)[CppIndex].ToString());
    }
    default:
        return sol::nil;
    }
}

void LuaArrayProxy::NewIndex(sol::this_state LuaState, LuaArrayProxy& Self, int Index, sol::object Value)
{
    if (!Self.Instance || !Self.Property) return;
    if (Self.Property->Type != EPropertyType::Array) return;

    // Convert 1-based Lua index to 0-based C++ index
    if (Index < 1) return;
    size_t CppIndex = static_cast<size_t>(Index - 1);

    // Handle UObject pointer arrays
    if (IsObjectPointerType(Self.Property->InnerType))
    {
        TArray<UObject*>* ArrayPtr = Self.Property->GetValuePtr<TArray<UObject*>>(Self.Instance);
        if (!ArrayPtr) return;

        // Resize if necessary
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, nullptr);

        // Handle nil assignment
        if (Value.get_type() == sol::type::nil || Value.get_type() == sol::type::none)
        {
            (*ArrayPtr)[CppIndex] = nullptr;
            return;
        }

        // Must be a proxy object
        if (!Value.is<LuaObjectProxy>()) return;

        LuaObjectProxy& Proxy = Value.as<LuaObjectProxy&>();
        UObject* Obj = Proxy.Instance;

        // Validate object
        if (Obj && !IsValidUObject(Obj)) return;

        (*ArrayPtr)[CppIndex] = Obj;
        return;
    }

    // Handle primitive type arrays
    switch (Self.Property->InnerType)
    {
    case EPropertyType::Int32:
    {
        TArray<int32>* ArrayPtr = Self.Property->GetValuePtr<TArray<int32>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, 0);
        if (Value.get_type() == sol::type::number)
            (*ArrayPtr)[CppIndex] = static_cast<int32>(Value.as<double>());
        break;
    }
    case EPropertyType::Float:
    {
        TArray<float>* ArrayPtr = Self.Property->GetValuePtr<TArray<float>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, 0.0f);
        if (Value.get_type() == sol::type::number)
            (*ArrayPtr)[CppIndex] = static_cast<float>(Value.as<double>());
        break;
    }
    case EPropertyType::Bool:
    {
        TArray<bool>* ArrayPtr = Self.Property->GetValuePtr<TArray<bool>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, false);
        if (Value.get_type() == sol::type::boolean)
            (*ArrayPtr)[CppIndex] = Value.as<bool>();
        break;
    }
    case EPropertyType::FString:
    {
        TArray<FString>* ArrayPtr = Self.Property->GetValuePtr<TArray<FString>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, FString());
        if (Value.get_type() == sol::type::string)
            (*ArrayPtr)[CppIndex] = Value.as<FString>();
        break;
    }
    case EPropertyType::FVector:
    {
        TArray<FVector>* ArrayPtr = Self.Property->GetValuePtr<TArray<FVector>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, FVector());
        if (Value.is<FVector>())
            (*ArrayPtr)[CppIndex] = Value.as<FVector>();
        break;
    }
    case EPropertyType::FLinearColor:
    {
        TArray<FLinearColor>* ArrayPtr = Self.Property->GetValuePtr<TArray<FLinearColor>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, FLinearColor());
        if (Value.is<FLinearColor>())
            (*ArrayPtr)[CppIndex] = Value.as<FLinearColor>();
        break;
    }
    case EPropertyType::FName:
    {
        TArray<FName>* ArrayPtr = Self.Property->GetValuePtr<TArray<FName>>(Self.Instance);
        if (!ArrayPtr) return;
        if (CppIndex >= ArrayPtr->size())
            ArrayPtr->resize(CppIndex + 1, FName());
        if (Value.get_type() == sol::type::string)
            (*ArrayPtr)[CppIndex] = FName(Value.as<FString>());
        break;
    }
    default:
        break;
    }
}

size_t LuaArrayProxy::Length(LuaArrayProxy& Self)
{
    if (!Self.Instance || !Self.Property) return 0;
    if (Self.Property->Type != EPropertyType::Array) return 0;

    // Handle UObject pointer arrays
    if (IsObjectPointerType(Self.Property->InnerType))
    {
        TArray<UObject*>* ArrayPtr = Self.Property->GetValuePtr<TArray<UObject*>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }

    // Handle primitive type arrays
    switch (Self.Property->InnerType)
    {
    case EPropertyType::Int32:
    {
        TArray<int32>* ArrayPtr = Self.Property->GetValuePtr<TArray<int32>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::Float:
    {
        TArray<float>* ArrayPtr = Self.Property->GetValuePtr<TArray<float>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::Bool:
    {
        TArray<bool>* ArrayPtr = Self.Property->GetValuePtr<TArray<bool>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::FString:
    {
        TArray<FString>* ArrayPtr = Self.Property->GetValuePtr<TArray<FString>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::FVector:
    {
        TArray<FVector>* ArrayPtr = Self.Property->GetValuePtr<TArray<FVector>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::FLinearColor:
    {
        TArray<FLinearColor>* ArrayPtr = Self.Property->GetValuePtr<TArray<FLinearColor>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    case EPropertyType::FName:
    {
        TArray<FName>* ArrayPtr = Self.Property->GetValuePtr<TArray<FName>>(Self.Instance);
        return ArrayPtr ? ArrayPtr->size() : 0;
    }
    default:
        return 0;
    }
}

void LuaArrayProxy::RegisterLua(sol::state_view LuaState)
{
    LuaState.new_usertype<LuaArrayProxy>("LuaArrayProxy",
        sol::meta_function::index, &LuaArrayProxy::Index,
        sol::meta_function::new_index, &LuaArrayProxy::NewIndex,
        sol::meta_function::length, &LuaArrayProxy::Length
    );
}
