#include "pch.h"
#include "LuaScriptComponent.h"
#include <sol/state.hpp>
#include <sol/coroutine.hpp>

IMPLEMENT_CLASS(ULuaScriptComponent)

BEGIN_PROPERTIES(ULuaScriptComponent)
	MARK_AS_COMPONENT("Lua 스크립트 컴포넌트", "Lua 스크립트 컴포넌트입니다.")
	ADD_PROPERTY_SCRIPT(FString, ScriptFilePath, "Script", ".lua", true, "Lua Script 파일 경로")
END_PROPERTIES()

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
	// Todo : 순서 강제성 주입
	CoroutineManager.ShutdownBeforeLuaClose(); // delete Lua보다 선행 되어야 함
	
	if (Lua)
	{
		delete Lua;
		Lua = nullptr;
	}
}

void ULuaScriptComponent::BeginPlay()
{
	if (ScriptFilePath.empty())
	{
		return;
	}

	Lua = new sol::state();

	Lua->open_libraries(sol::lib::base, sol::lib::coroutine);

	Lua->set_function("print", sol::overload(
		[](const FString& msg) {
			UE_LOG("[Lua-Str] %s\n", msg.c_str());
		},
		[](int num){
			UE_LOG("[Lua] %d\n", num);
		}
	));
	
	Lua->new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
	);

	Lua->new_usertype<FGameObject>("GameObject",
		"UUID", &FGameObject::UUID,
		"Location", &FGameObject::Location,
		"Velocity", &FGameObject::Velocity,
		"PrintLocation", &FGameObject::PrintLocation
	);
	
	FGameObject* Obj = Owner->GetGameObject();
	(*Lua)["Obj"] = Obj;

	try
	{
		Lua->script_file(ScriptFilePath);
	}
	catch (const sol::error& Err)
	{
		UE_LOG("[error] %s", Err.what());
		GEngine.EndPIE();
		return;
	}
	
	sol::function AI = (*Lua)["AI"].get<sol::function>();
	if (!AI.valid()) { UE_LOG("AI not found\n"); return; }
	
	sol::coroutine co((*Lua), AI);
	
	CoroutineManager.AddCoroutine(std::move(co));

	sol::function AI2 = (*Lua)["AI2"].get<sol::function>();
	if (!AI2.valid()) { UE_LOG("AI2 not found\n"); return; }

	sol::coroutine co2((*Lua), AI2);
	
	CoroutineManager.AddCoroutine(std::move(co2));
	
    (*Lua)["BeginPlay"]();
}

void ULuaScriptComponent::OnOverlap(const AActor* Other)
{
	if (Lua)
	{
		(*Lua)["OnOverlap"](Other->GetGameObject());
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
	if (Lua)
	{
		(*Lua)["Tick"](DeltaTime);
		CoroutineManager.Tick(DeltaTime);
	}
}

void ULuaScriptComponent::EndPlay(EEndPlayReason Reason)
{
	if (Lua)
	{
		(*Lua)["EndPlay"]();
	}
}
