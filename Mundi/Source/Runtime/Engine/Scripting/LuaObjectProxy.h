#pragma once

#include <functional>
#include <sol/sol.hpp>

#include "LuaBindingRegistry.h"

struct FBoundProp
{
    const FProperty* Property = nullptr;  // TODO: editable/readonly flags...
};

struct FBoundClassDesc   // Property list per class
{
    UClass* Class = nullptr;
    TMap<FString, FBoundProp> PropsByName;
};

extern TMap<UClass*, FBoundClassDesc> GBoundClasses;

void BuildBoundClass(UClass* Class);

// Renamed from LuaComponentProxy to LuaObjectProxy
// Now handles all UObject types, not just components
struct LuaObjectProxy
{
    UObject* Instance = nullptr;  // Changed from void* for type safety
    UClass* Class = nullptr;

    // Validate if the UObject instance is still valid
    bool IsValid() const;

    static sol::object Index(sol::this_state LuaState, LuaObjectProxy& Self, const char* Key);
    static void        NewIndex(LuaObjectProxy& Self, const char* Key, sol::object Obj);
};

// Compatibility alias for existing code
using LuaComponentProxy = LuaObjectProxy;
