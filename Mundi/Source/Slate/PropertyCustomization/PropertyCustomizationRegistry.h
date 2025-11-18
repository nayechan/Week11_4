#pragma once

#include "IDetailCustomization.h"
#include "UEContainer.h"
#include <functional>

// Factory 함수 타입
using FDetailCustomizationFactory = std::function<IDetailCustomization*()>;

/**
 * RAII 래퍼 - IDetailCustomization 자동 정리
 * 예외 발생 시에도 메모리 누수를 방지
 */
class FCustomizationGuard
{
public:
	explicit FCustomizationGuard(IDetailCustomization* InCustomization)
		: Customization(InCustomization)
	{
	}

	~FCustomizationGuard()
	{
		if (Customization)
		{
			delete Customization;
			Customization = nullptr;
		}
	}

	IDetailCustomization* Get() const { return Customization; }
	IDetailCustomization* operator->() const { return Customization; }
	explicit operator bool() const { return Customization != nullptr; }

	// 복사 불가
	FCustomizationGuard(const FCustomizationGuard&) = delete;
	FCustomizationGuard& operator=(const FCustomizationGuard&) = delete;

private:
	IDetailCustomization* Customization;
};

/**
 * Property Customization Registry
 * 클래스별 커스터마이제이션 등록 및 조회
 */
class UPropertyCustomizationRegistry
{
public:
	/**
	 * Customization 등록
	 * @param ClassName - 대상 클래스 이름 (예: "USkeletalMeshComponent")
	 * @param Factory - Customization 인스턴스 생성 함수
	 */
	static void RegisterCustomization(const char* ClassName, FDetailCustomizationFactory Factory);

	/**
	 * Customization 조회
	 * @param ClassName - 대상 클래스 이름
	 * @return Customization 인스턴스 (없으면 nullptr)
	 */
	static IDetailCustomization* FindCustomization(const char* ClassName);

	/**
	 * 모든 등록 해제 (엔진 종료 시)
	 */
	static void UnregisterAll();

private:
	// 클래스 이름 -> Factory 맵
	static TMap<FString, FDetailCustomizationFactory> Customizations;
};
