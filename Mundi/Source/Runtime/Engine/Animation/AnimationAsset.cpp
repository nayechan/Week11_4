#include "pch.h"
#include "AnimationAsset.h"
#include "GlobalConsole.h"

void UAnimationAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 현재는 비어있음 (스켈레톤은 참조만 저장)
	// TODO: 스켈레톤 경로 직렬화
}
