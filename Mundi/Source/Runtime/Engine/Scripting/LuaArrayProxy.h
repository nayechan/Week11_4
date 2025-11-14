#pragma once

#include "sol/sol.hpp"
#include "Property.h"

// Forward declaration
struct LuaObjectProxy;

/**
 * Lua proxy for TArray<T> properties
 * Provides Lua-side array access with 1-based indexing
 * Supports primitive types and UObject pointers
 */
class LuaArrayProxy
{
public:
    void* Instance = nullptr;              // Object instance containing the array
    const FProperty* Property = nullptr;   // Array property metadata (contains InnerType)

    LuaArrayProxy() = default;
    LuaArrayProxy(void* InInstance, const FProperty* InProperty)
        : Instance(InInstance), Property(InProperty)
    {
    }

    // Lua metamethods
    static sol::object Index(sol::this_state LuaState, LuaArrayProxy& Self, int Index);
    static void NewIndex(sol::this_state LuaState, LuaArrayProxy& Self, int Index, sol::object Value);
    static size_t Length(LuaArrayProxy& Self);

    // Lua registration
    static void RegisterLua(sol::state_view LuaState);
};
