#include "pch.h"
#include "LightInfo.h"
#include "AmbientLightComponent.h"
#include "DirectionalLightComponent.h"
#include "SpotLightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"

void FLightManager::UpdateLightBuffer(D3D11RHI* RHIDevice)
{

	if (bHaveToUpdate)
	{
		FLightBufferType LightBuffer{};

		if (AmbientLightList[0])
		{
			LightBuffer.AmbientLight = AmbientLightList[0]->GetLightInfo();
		}
		if (DIrectionalLightList[0])
		{
			LightBuffer.DirectionalLight = DIrectionalLightList[0]->GetLightInfo();
		}

		if (bPointLightDirty)
		{
			//Update Structuredbuffer
			bPointLightDirty = false;
		}
		if (bSpotLightDirty)
		{
			//Update StructuredBuffer
			bSpotLightDirty = false;
		}

		LightBuffer.PointLightCount = PointLightNum;
		LightBuffer.SpotLightCount = SpotLightNum;
		RHIDevice->SetAndUpdateConstantBuffer(LightBuffer);

		bHaveToUpdate = false;
	}
}

void FLightManager::RegisterLight(ULightComponent* LightComponent, ELightType Type)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	switch (Type)
	{
	case ELightType::AmbientLight:
		AmbientLightList.Add(static_cast<UAmbientLightComponent*>(LightComponent));
		break;
	case ELightType::DirectionalLight:
		DIrectionalLightList.Add(static_cast<UDirectionalLightComponent*>(LightComponent));
		break;
	case ELightType::PointLight:
	{
		PointLightList.Add(static_cast<UPointLightComponent*>(LightComponent));
		PointLightNum++;
		bPointLightDirty = true;
		break;
	}
	case ELightType::SpotLight:
	{
		SpotLightList.Add(static_cast<USpotLightComponent*>(LightComponent));
		SpotLightNum++;
		bSpotLightDirty = true;
	}
	}
}

void FLightManager::DeRegisterLight(ULightComponent* LightComponent, ELightType Type)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);

	switch (Type)
	{
	case ELightType::AmbientLight:
		AmbientLightList.Remove(static_cast<UAmbientLightComponent*>(LightComponent));
		break;
	case ELightType::DirectionalLight:
		DIrectionalLightList.Remove(static_cast<UDirectionalLightComponent*>(LightComponent));
		break;
	case ELightType::PointLight:
	{
		PointLightList.Remove(static_cast<UPointLightComponent*>(LightComponent));
		PointLightNum--;
		bPointLightDirty = true;
		break;
	}
	case ELightType::SpotLight:
	{
		SpotLightList.Remove(static_cast<USpotLightComponent*>(LightComponent));
		SpotLightNum--;
		bSpotLightDirty = true;
	}
	}
}
