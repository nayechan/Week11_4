#include "pch.h"
#include "LuaMapProxy.h"
#include "LuaObjectProxy.h"
#include "LuaObjectProxyHelpers.h"
#include "ObjectFactory.h"

// External function from LuaManager.cpp
extern sol::object MakeCompProxy(sol::state_view SolState, void* Instance, UClass* Class);

// ===== Helper Functions =====

// Convert C++ value to Lua object based on EPropertyType
static sol::object ConvertValueToLua(sol::state_view LuaView, EPropertyType ValueType, void* ValuePtr)
{
    if (IsObjectPointerType(ValueType))
    {
        UObject* Obj = *static_cast<UObject**>(ValuePtr);
        if (IsValidUObject(Obj))
            return MakeCompProxy(LuaView, Obj, Obj->GetClass());
        return sol::nil;
    }

    switch (ValueType)
    {
    case EPropertyType::Bool:         return sol::make_object(LuaView, *static_cast<bool*>(ValuePtr));
    case EPropertyType::Int32:        return sol::make_object(LuaView, *static_cast<int32*>(ValuePtr));
    case EPropertyType::Float:        return sol::make_object(LuaView, *static_cast<float*>(ValuePtr));
    case EPropertyType::FString:      return sol::make_object(LuaView, *static_cast<FString*>(ValuePtr));
    case EPropertyType::FVector:      return sol::make_object(LuaView, *static_cast<FVector*>(ValuePtr));
    case EPropertyType::FLinearColor: return sol::make_object(LuaView, *static_cast<FLinearColor*>(ValuePtr));
    case EPropertyType::FName:        return sol::make_object(LuaView, static_cast<FName*>(ValuePtr)->ToString());
    default:                          return sol::nil;
    }
}

// ===== Lua Metamethods =====

sol::object LuaMapProxy::Index(sol::this_state LuaState, LuaMapProxy& Self, sol::object Key)
{
    if (!Self.Instance || !Self.Property) return sol::nil;
    if (Self.Property->Type != EPropertyType::Map) return sol::nil;

    sol::state_view LuaView(LuaState);

    // Macro to handle map lookup for specific <K,V> combination
    #define HANDLE_MAP_GET(KeyCppType, ValueCppType, KeyConverter) \
    { \
        auto* MapPtr = Self.Property->GetValuePtr<TMap<KeyCppType, ValueCppType>>(Self.Instance); \
        if (!MapPtr) return sol::nil; \
        \
        KeyCppType CppKey = KeyConverter; \
        auto It = MapPtr->find(CppKey); \
        if (It == MapPtr->end()) return sol::nil; \
        \
        ValueCppType& Value = It->second; \
        return ConvertValueToLua(LuaView, Self.Property->InnerType, &Value); \
    }

    // Handle different KeyType combinations
    if (Self.Property->KeyType == EPropertyType::FString)
    {
        if (Key.get_type() != sol::type::string) return sol::nil;
        FString LuaKey = Key.as<FString>();

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:        HANDLE_MAP_GET(FString, int32, LuaKey);
        case EPropertyType::Float:        HANDLE_MAP_GET(FString, float, LuaKey);
        case EPropertyType::Bool:         HANDLE_MAP_GET(FString, bool, LuaKey);
        case EPropertyType::FString:      HANDLE_MAP_GET(FString, FString, LuaKey);
        case EPropertyType::FVector:      HANDLE_MAP_GET(FString, FVector, LuaKey);
        case EPropertyType::FLinearColor: HANDLE_MAP_GET(FString, FLinearColor, LuaKey);
        case EPropertyType::FName:        HANDLE_MAP_GET(FString, FName, LuaKey);
        case EPropertyType::ObjectPtr:    HANDLE_MAP_GET(FString, UObject*, LuaKey);
        default: return sol::nil;
        }
    }
    else if (Self.Property->KeyType == EPropertyType::Int32)
    {
        if (Key.get_type() != sol::type::number) return sol::nil;
        int32 LuaKey = static_cast<int32>(Key.as<double>());

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:        HANDLE_MAP_GET(int32, int32, LuaKey);
        case EPropertyType::Float:        HANDLE_MAP_GET(int32, float, LuaKey);
        case EPropertyType::Bool:         HANDLE_MAP_GET(int32, bool, LuaKey);
        case EPropertyType::FString:      HANDLE_MAP_GET(int32, FString, LuaKey);
        case EPropertyType::FVector:      HANDLE_MAP_GET(int32, FVector, LuaKey);
        case EPropertyType::FLinearColor: HANDLE_MAP_GET(int32, FLinearColor, LuaKey);
        case EPropertyType::FName:        HANDLE_MAP_GET(int32, FName, LuaKey);
        case EPropertyType::ObjectPtr:    HANDLE_MAP_GET(int32, UObject*, LuaKey);
        default: return sol::nil;
        }
    }
    else if (Self.Property->KeyType == EPropertyType::FName)
    {
        if (Key.get_type() != sol::type::string) return sol::nil;
        FName LuaKey = FName(Key.as<FString>());

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:        HANDLE_MAP_GET(FName, int32, LuaKey);
        case EPropertyType::Float:        HANDLE_MAP_GET(FName, float, LuaKey);
        case EPropertyType::Bool:         HANDLE_MAP_GET(FName, bool, LuaKey);
        case EPropertyType::FString:      HANDLE_MAP_GET(FName, FString, LuaKey);
        case EPropertyType::FVector:      HANDLE_MAP_GET(FName, FVector, LuaKey);
        case EPropertyType::FLinearColor: HANDLE_MAP_GET(FName, FLinearColor, LuaKey);
        case EPropertyType::FName:        HANDLE_MAP_GET(FName, FName, LuaKey);
        case EPropertyType::ObjectPtr:    HANDLE_MAP_GET(FName, UObject*, LuaKey);
        default: return sol::nil;
        }
    }

    #undef HANDLE_MAP_GET
    return sol::nil;
}

void LuaMapProxy::NewIndex(sol::this_state LuaState, LuaMapProxy& Self, sol::object Key, sol::object Value)
{
    if (!Self.Instance || !Self.Property) return;
    if (Self.Property->Type != EPropertyType::Map) return;

    // Macro to handle map assignment for specific <K,V> combination
    #define HANDLE_MAP_SET(KeyCppType, ValueCppType, KeyConverter, ValueConverter, NilCheck) \
    { \
        auto* MapPtr = Self.Property->GetValuePtr<TMap<KeyCppType, ValueCppType>>(Self.Instance); \
        if (!MapPtr) return; \
        \
        KeyCppType CppKey = KeyConverter; \
        \
        /* Handle nil assignment (remove key) */ \
        if (Value.get_type() == sol::type::nil || Value.get_type() == sol::type::none) \
        { \
            MapPtr->erase(CppKey); \
            return; \
        } \
        \
        /* Validate value type */ \
        if (!(NilCheck)) return; \
        \
        ValueCppType CppValue = ValueConverter; \
        (*MapPtr)[CppKey] = CppValue; \
        return; \
    }

    // Handle different KeyType combinations
    if (Self.Property->KeyType == EPropertyType::FString)
    {
        if (Key.get_type() != sol::type::string) return;
        FString LuaKey = Key.as<FString>();

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:
            HANDLE_MAP_SET(FString, int32, LuaKey,
                static_cast<int32>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Float:
            HANDLE_MAP_SET(FString, float, LuaKey,
                static_cast<float>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Bool:
            HANDLE_MAP_SET(FString, bool, LuaKey,
                Value.as<bool>(),
                Value.get_type() == sol::type::boolean);
        case EPropertyType::FString:
            HANDLE_MAP_SET(FString, FString, LuaKey,
                Value.as<FString>(),
                Value.get_type() == sol::type::string);
        case EPropertyType::FVector:
            HANDLE_MAP_SET(FString, FVector, LuaKey,
                Value.as<FVector>(),
                Value.is<FVector>());
        case EPropertyType::FLinearColor:
            HANDLE_MAP_SET(FString, FLinearColor, LuaKey,
                Value.as<FLinearColor>(),
                Value.is<FLinearColor>());
        case EPropertyType::FName:
            HANDLE_MAP_SET(FString, FName, LuaKey,
                FName(Value.as<FString>()),
                Value.get_type() == sol::type::string);
        case EPropertyType::ObjectPtr:
        {
            if (!Value.is<LuaObjectProxy>()) return;
            UObject* Obj = Value.as<LuaObjectProxy&>().Instance;
            if (Obj && !IsValidUObject(Obj)) return;
            HANDLE_MAP_SET(FString, UObject*, LuaKey, Obj, true);
        }
        default: return;
        }
    }
    else if (Self.Property->KeyType == EPropertyType::Int32)
    {
        if (Key.get_type() != sol::type::number) return;
        int32 LuaKey = static_cast<int32>(Key.as<double>());

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:
            HANDLE_MAP_SET(int32, int32, LuaKey,
                static_cast<int32>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Float:
            HANDLE_MAP_SET(int32, float, LuaKey,
                static_cast<float>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Bool:
            HANDLE_MAP_SET(int32, bool, LuaKey,
                Value.as<bool>(),
                Value.get_type() == sol::type::boolean);
        case EPropertyType::FString:
            HANDLE_MAP_SET(int32, FString, LuaKey,
                Value.as<FString>(),
                Value.get_type() == sol::type::string);
        case EPropertyType::FVector:
            HANDLE_MAP_SET(int32, FVector, LuaKey,
                Value.as<FVector>(),
                Value.is<FVector>());
        case EPropertyType::FLinearColor:
            HANDLE_MAP_SET(int32, FLinearColor, LuaKey,
                Value.as<FLinearColor>(),
                Value.is<FLinearColor>());
        case EPropertyType::FName:
            HANDLE_MAP_SET(int32, FName, LuaKey,
                FName(Value.as<FString>()),
                Value.get_type() == sol::type::string);
        case EPropertyType::ObjectPtr:
        {
            if (!Value.is<LuaObjectProxy>()) return;
            UObject* Obj = Value.as<LuaObjectProxy&>().Instance;
            if (Obj && !IsValidUObject(Obj)) return;
            HANDLE_MAP_SET(int32, UObject*, LuaKey, Obj, true);
        }
        default: return;
        }
    }
    else if (Self.Property->KeyType == EPropertyType::FName)
    {
        if (Key.get_type() != sol::type::string) return;
        FName LuaKey = FName(Key.as<FString>());

        switch (Self.Property->InnerType)
        {
        case EPropertyType::Int32:
            HANDLE_MAP_SET(FName, int32, LuaKey,
                static_cast<int32>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Float:
            HANDLE_MAP_SET(FName, float, LuaKey,
                static_cast<float>(Value.as<double>()),
                Value.get_type() == sol::type::number);
        case EPropertyType::Bool:
            HANDLE_MAP_SET(FName, bool, LuaKey,
                Value.as<bool>(),
                Value.get_type() == sol::type::boolean);
        case EPropertyType::FString:
            HANDLE_MAP_SET(FName, FString, LuaKey,
                Value.as<FString>(),
                Value.get_type() == sol::type::string);
        case EPropertyType::FVector:
            HANDLE_MAP_SET(FName, FVector, LuaKey,
                Value.as<FVector>(),
                Value.is<FVector>());
        case EPropertyType::FLinearColor:
            HANDLE_MAP_SET(FName, FLinearColor, LuaKey,
                Value.as<FLinearColor>(),
                Value.is<FLinearColor>());
        case EPropertyType::FName:
            HANDLE_MAP_SET(FName, FName, LuaKey,
                FName(Value.as<FString>()),
                Value.get_type() == sol::type::string);
        case EPropertyType::ObjectPtr:
        {
            if (!Value.is<LuaObjectProxy>()) return;
            UObject* Obj = Value.as<LuaObjectProxy&>().Instance;
            if (Obj && !IsValidUObject(Obj)) return;
            HANDLE_MAP_SET(FName, UObject*, LuaKey, Obj, true);
        }
        default: return;
        }
    }

    #undef HANDLE_MAP_SET
}

void LuaMapProxy::RegisterLua(sol::state_view LuaState)
{
    LuaState.new_usertype<LuaMapProxy>("LuaMapProxy",
        sol::meta_function::index, &LuaMapProxy::Index,
        sol::meta_function::new_index, &LuaMapProxy::NewIndex
    );
}
