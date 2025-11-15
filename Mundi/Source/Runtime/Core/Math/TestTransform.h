#pragma once

#include "Source/Runtime/Core/Math/Vector.h"
#include "FTestTransform.generated.h"

// USTRUCT 리플렉션 테스트용 구조체
// 이 구조체는 Component에서 사용되며, Lua에서 접근 가능해야 합니다.
USTRUCT(DisplayName="테스트 트랜스폼", Description="USTRUCT 리플렉션 기능을 테스트하기 위한 구조체")
struct FTestTransform
{
	GENERATED_REFLECTION_BODY()

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transform", Tooltip="위치 벡터")
	FVector Position = FVector(0, 0, 0);

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transform", Tooltip="회전 벡터")
	FVector Rotation = FVector(0, 0, 0);

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transform", Tooltip="스케일 벡터")
	FVector Scale = FVector(1, 1, 2);

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transform", Tooltip="알파 값", Range="0,1")
	float Alpha = 1.0f;
};
