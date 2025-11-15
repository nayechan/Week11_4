#pragma once

#include "Object.h"
#include "SceneComponent.h"
#include "Source/Runtime/Core/Math/TestTransform.h"
#include "UTestComponent.generated.h"

/**
 * 범용 테스트 컴포넌트
 * - USTRUCT 리플렉션 테스트
 * - Lua 스크립트 접근 테스트
 * - 향후 다양한 기능 테스트에 활용 가능
 */
UCLASS(DisplayName="테스트 컴포넌트", Description="범용 테스트 컴포넌트입니다")
class UTestComponent : public USceneComponent
{
	GENERATED_REFLECTION_BODY()

public:
	UTestComponent() = default;
	virtual ~UTestComponent() = default;

	// ===== USTRUCT 테스트용 =====

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Test|Struct", Tooltip="테스트용 트랜스폼 구조체")
	FTestTransform TestTransform;

	// ===== 기본 프로퍼티 테스트용 =====

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Test|Basic", Tooltip="테스트 플래그")
	bool bEnabled = true;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Test|Basic", Range="0,100", Tooltip="테스트 강도")
	float Intensity = 50.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Test|Basic", Tooltip="테스트 메시지")
	FString Message = "Hello from UTestComponent!";

	// ===== 테스트 메서드 =====

	UFUNCTION(LuaBind, DisplayName="PrintTransform")
	void PrintTransform()
	{
		UE_LOG("[UTestComponent] Transform - Pos:(%.1f,%.1f,%.1f) Rot:(%.1f,%.1f,%.1f) Scale:(%.1f,%.1f,%.1f) Alpha:%.2f",
			TestTransform.Position.X, TestTransform.Position.Y, TestTransform.Position.Z,
			TestTransform.Rotation.X, TestTransform.Rotation.Y, TestTransform.Rotation.Z,
			TestTransform.Scale.X, TestTransform.Scale.Y, TestTransform.Scale.Z,
			TestTransform.Alpha);
	}

	UFUNCTION(LuaBind, DisplayName="ResetTransform")
	void ResetTransform()
	{
		TestTransform.Position = FVector(0, 0, 0);
		TestTransform.Rotation = FVector(0, 0, 0);
		TestTransform.Scale = FVector(1, 1, 1);
		TestTransform.Alpha = 1.0f;
		UE_LOG("[UTestComponent] Transform reset to default");
	}
};
