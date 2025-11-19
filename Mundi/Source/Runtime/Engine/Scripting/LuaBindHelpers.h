#pragma once
#include "LuaManager.h"
#include "LuaObjectProxy.h"  // Changed from LuaComponentProxy.h

// 클래스별 바인더 등록 매크로(컴포넌트 cpp에서 사용)
#define LUA_BIND_BEGIN(ClassType) \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T); \
struct FLuaBinder_##ClassType { \
FLuaBinder_##ClassType() { \
FLuaBindRegistry::Get().Register(ClassType::StaticClass(), &BuildLua_##ClassType); \
} \
}; \
static FLuaBinder_##ClassType G_LuaBinder_##ClassType; \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T) \
/**/

#define LUA_BIND_END() /* nothing */

// 멤버 함수 포인터를 Lua 함수로 감싸는 헬퍼(리턴 void 버전)
template<typename C, typename... P>
static void AddMethod(sol::table& T, const char* Name, void(C::*Method)(P...))
{
    T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args)
    {
        if (!Proxy.Instance) return;
        // 상속 지원: IsChildOf로 호환성 체크 (Unreal 패턴)
        if (!Proxy.Class->IsChildOf(C::StaticClass())) return;
        (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
    });
}

// 리턴값이 있는 멤버 함수용 버전
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...))
{
    T.set_function(Name, [Method](sol::this_state LuaState, LuaComponentProxy& Proxy, P... Args) -> sol::object
    {
        if (!Proxy.Instance || !Proxy.Class->IsChildOf(C::StaticClass()))
        {
            if constexpr (!std::is_void_v<R>)
            {
                sol::state_view LuaView(LuaState);
                return sol::nil;
            }
        }

        R Result = (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);

        // ⭐ UObject* 타입이면 MakeCompProxy로 래핑 (NewObject 패턴과 동일)
        if constexpr (std::is_pointer_v<R> && std::is_base_of_v<UObject, std::remove_pointer_t<R>>)
        {
            sol::state_view LuaView(LuaState);
            if (Result)
            {
                // MakeCompProxy는 LuaManager.cpp에 extern 선언됨
                extern sol::object MakeCompProxy(sol::state_view, void*, UClass*);
                return MakeCompProxy(LuaView, Result, Result->GetClass());
            }
            return sol::nil;
        }
        else
        {
            // 일반 타입은 그대로 반환
            sol::state_view LuaView(LuaState);
            return sol::make_object(LuaView, Result);
        }
    });
}

// const 멤버 함수용 오버로드
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...) const)
{
    T.set_function(Name, [Method](sol::this_state LuaState, LuaComponentProxy& Proxy, P... Args) -> sol::object
    {
        if (!Proxy.Instance || !Proxy.Class->IsChildOf(C::StaticClass()))
        {
            if constexpr (!std::is_void_v<R>)
            {
                sol::state_view LuaView(LuaState);
                return sol::nil;
            }
        }

        R Result = (static_cast<const C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);

        // ⭐ UObject* 타입이면 MakeCompProxy로 래핑 (NewObject 패턴과 동일)
        if constexpr (std::is_pointer_v<R> && std::is_base_of_v<UObject, std::remove_pointer_t<R>>)
        {
            sol::state_view LuaView(LuaState);
            if (Result)
            {
                extern sol::object MakeCompProxy(sol::state_view, void*, UClass*);
                return MakeCompProxy(LuaView, Result, Result->GetClass());
            }
            return sol::nil;
        }
        else
        {
            // 일반 타입은 그대로 반환
            sol::state_view LuaView(LuaState);
            return sol::make_object(LuaView, Result);
        }
    });
}

// 친절한 별칭 부여용
template<typename C, typename... P>
static void AddAlias(sol::table& T, const char* Alias, void(C::*Method)(P...))
{
    AddMethod<C, P...>(T, Alias, Method);
}
