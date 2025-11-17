#pragma once

#include "Delegates.h"
#include "UEContainer.h"

/**
 * 애셋 필터링 델리게이트
 * @param AssetPath - 애셋 경로
 * @return true면 필터링(숨김), false면 표시
 */
DECLARE_FUNCTION_RetVal_OneParam(FOnShouldFilterAsset, bool, const FString& /*AssetPath*/);

/**
 * 프로퍼티 필터링 델리게이트
 * @param PropertyName - 프로퍼티 이름
 * @return true면 필터링(숨김), false면 표시
 */
DECLARE_FUNCTION_RetVal_OneParam(FOnShouldFilterProperty, bool, const FString& /*PropertyName*/);
