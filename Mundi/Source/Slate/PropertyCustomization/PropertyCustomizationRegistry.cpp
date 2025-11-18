#include "pch.h"
#include "PropertyCustomizationRegistry.h"
#include "GlobalConsole.h"

// Static 멤버 초기화
TMap<FString, FDetailCustomizationFactory> UPropertyCustomizationRegistry::Customizations;

void UPropertyCustomizationRegistry::RegisterCustomization(const char* ClassName, FDetailCustomizationFactory Factory)
{
	if (!ClassName || !Factory)
	{
		UE_LOG("PropertyCustomizationRegistry: Invalid registration (ClassName or Factory is null)");
		return;
	}

	FString Key = ClassName;
	Customizations[Key] = Factory;

	UE_LOG("Registered property customization: %s", ClassName);
}

IDetailCustomization* UPropertyCustomizationRegistry::FindCustomization(const char* ClassName)
{
	if (!ClassName)
		return nullptr;

	FString Key = ClassName;
	auto It = Customizations.Find(Key);

	if (It != nullptr)
	{
		// Factory 호출하여 인스턴스 생성
		return (*It)();
	}

	return nullptr;
}

void UPropertyCustomizationRegistry::UnregisterAll()
{
	Customizations.Empty();
	UE_LOG("All property customizations unregistered");
}
