#pragma once

#include "sol/sol.hpp"
#include "Object.h"
#include "Property.h"

/**
 * Lua에서 C++ struct에 접근할 수 있게 해주는 프록시
 * UObject가 아닌 일반 struct (USTRUCT)를 Lua에 노출
 *
 * LuaObjectProxy와 유사하지만:
 * - UClass 대신 UStruct* 사용
 * - UObject* 대신 void* 사용 (struct는 값 타입)
 */
struct LuaStructProxy
{
	void* Instance = nullptr;      // Struct 인스턴스 포인터
	UStruct* StructType = nullptr; // Struct 메타데이터 (FTestTransform 등)

	LuaStructProxy() = default;
	LuaStructProxy(void* InInstance, UStruct* InStructType)
		: Instance(InInstance), StructType(InStructType) {}

	// Lua __index metamethod: struct.PropertyName → 값 반환
	static sol::object Index(sol::this_state LuaState, LuaStructProxy& Self, const char* Key);

	// Lua __newindex metamethod: struct.PropertyName = value
	static void NewIndex(LuaStructProxy& Self, const char* Key, sol::object Value);

	// Lua registration
	static void RegisterLua(sol::state_view LuaState);
};
