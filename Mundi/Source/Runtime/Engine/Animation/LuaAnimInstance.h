#pragma once

#include "AnimInstance.h"
#include "ULuaAnimInstance.generated.h"

UCLASS(DisplayName="Lua 애니메이션 인스턴스", Description="Lua 스크립트로 제어되는 애니메이션 인스턴스입니다")
class ULuaAnimInstance : public UAnimInstance
{
public:
	GENERATED_REFLECTION_BODY()

	ULuaAnimInstance();
	~ULuaAnimInstance() override;

public:
	void LuaInitializeAnimation() override;
	void LuaUpdateAnimation(float DeltaSeconds) override;
	void GetAnimationPose(FPoseContext& OutPose) override;

	void CleanupLuaResources();

	// Lua 스크립트 파일 경로 (에디터에서 설정 또는 런타임에서 동적 설정)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation|Lua", Tooltip="Lua 스크립트 파일 경로입니다")
	FString LuaScriptPath{};

	// ========================================
	// Gameplay Variables Dictionary
	// ========================================
	// String-based dictionary for Lua flexibility
	// Lua에서 파싱: tonumber(Variables["Speed"]), Variables["Name"] 등
	UPROPERTY(LuaReadWrite, Category="Animation|Lua", Tooltip="게임플레이 변수 딕셔너리 (String 기반)")
	TMap<FString, FString> Variables{};

protected:

	// ⭐ ULuaScriptComponent 패턴: sol2가 pch.h에 있어서 값 멤버 사용 가능
	sol::state* Lua = nullptr;
	sol::environment Env{};

	// 함수 캐시
	sol::protected_function LuaInitFunc{};
	sol::protected_function LuaUpdateFunc{};
	sol::protected_function LuaGetPoseFunc{};

	bool bIsLuaInitialized = false;
	bool bIsLuaCleanedUp = false;
};
