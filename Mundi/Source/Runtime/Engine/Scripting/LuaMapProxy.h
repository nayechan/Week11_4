#pragma once

#include "sol/sol.hpp"
#include "Property.h"

// Forward declaration
struct LuaObjectProxy;

/**
 * Lua proxy for TMap<K, V> properties
 * Provides Lua-side map access with natural key-value syntax
 * Supports FString, int32, FName keys and various value types
 */
class LuaMapProxy
{
public:
    void* Instance = nullptr;              // Object instance containing the map
    const FProperty* Property = nullptr;   // Map property metadata (contains KeyType and InnerType)

    LuaMapProxy() = default;
    LuaMapProxy(void* InInstance, const FProperty* InProperty)
        : Instance(InInstance), Property(InProperty)
    {
    }

    // Lua metamethods
    static sol::object Index(sol::this_state LuaState, LuaMapProxy& Self, sol::object Key);
    static void NewIndex(sol::this_state LuaState, LuaMapProxy& Self, sol::object Key, sol::object Value);

    // Lua registration
    static void RegisterLua(sol::state_view LuaState);
};
