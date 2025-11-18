#pragma once

#include "Object.h"
#include "Delegates.h"

// Forward declarations
class UPropertyRenderer;

/**
 * Detail Customization 인터페이스
 * 특정 클래스의 프로퍼티 렌더링을 커스터마이징
 */
class IDetailCustomization
{
public:
	virtual ~IDetailCustomization() = default;

	/**
	 * 프로퍼티 렌더링 커스터마이징
	 * @param Object - 편집 중인 객체
	 */
	virtual void CustomizeDetails(UObject* Object) = 0;
};
