#pragma once

#include <sol/sol.hpp>

#include "Object.h"

class FLuaBindRegistry
{
public:
    ~FLuaBindRegistry() { FunctionTables.Empty(); Builders.Empty(); }
    using BuildFunc = void(*)(sol::state_view, sol::table&);
    
    static FLuaBindRegistry& Get()
    {
        static FLuaBindRegistry Singleton;
        return Singleton;
    }

    void Register(const UClass* Class, BuildFunc Func)
    {
        if (!Class || !Func) return;
        Builders.emplace(Class, Func);
    }

    const TMap<const UClass*, BuildFunc>& GetBuilders() const { return Builders; }

    // Build (or fetch) the function table for a given class (includes inheritance)
    sol::table& EnsureTable(sol::state_view Lua, const UClass* Class)
    {
        if (!Class) return Empty(Lua);

        if (auto It = FunctionTables.find(Class); It != FunctionTables.end())
            return It->second;

        sol::table ParentTable;
        if (const UClass* Super = Class->Super)
            ParentTable = EnsureTable(Lua, Super);

        sol::table MyTable = Lua.create_table();
        if (ParentTable.valid())
        {
            sol::table Meta = Lua.create_table();
            Meta[sol::meta_function::index] = ParentTable;
            MyTable[sol::metatable_key] = Meta;
        }

        if (auto ItB = Builders.find(Class); ItB != Builders.end())
            ItB->second(Lua, MyTable);

        auto [Inserted, _] = FunctionTables.emplace(Class, MyTable);
        return Inserted->second;
    }

    void Reset()
    {
        FunctionTables.Empty();
    }

private:
    TMap<const UClass*, BuildFunc> Builders;
    TMap<const UClass*, sol::table> FunctionTables;

    sol::table& Empty(sol::state_view L)
    {
        static std::unordered_map<lua_State*, sol::table> DummyPerState;
        auto* key = L.lua_state();
        auto it = DummyPerState.find(key);
        if (it == DummyPerState.end() || !it->second.valid())
            it = DummyPerState.emplace(key, L.create_table()).first;
        return it->second;
    }
};

