#include "pch.h"
#include "LightManager.h"
#include "AmbientLightComponent.h"
#include "DirectionalLightComponent.h"
#include "SpotLightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"

#define NUM_POINT_LIGHT_MAX 256
#define NUM_SPOT_LIGHT_MAX 256
FLightManager::~FLightManager()
{
	Release();
}
void FLightManager::Initialize(D3D11RHI* RHIDevice)
{
	if (!PointLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FPointLightInfo), NUM_POINT_LIGHT_MAX, nullptr, &PointLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(PointLightBuffer, &PointLightBufferSRV);
	}
	if (!SpotLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FSpotLightInfo), NUM_SPOT_LIGHT_MAX, nullptr, &SpotLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(SpotLightBuffer, &SpotLightBufferSRV);
	}

}
void FLightManager::Release()
{
	if (PointLightBuffer)
	{
		PointLightBuffer->Release();
		PointLightBuffer = nullptr;
	}
	if (PointLightBufferSRV)
	{
		PointLightBufferSRV->Release();
		PointLightBufferSRV = nullptr;
	}

	if (SpotLightBuffer)
	{
		SpotLightBuffer->Release();
		SpotLightBuffer = nullptr;
	}
	if (SpotLightBufferSRV)
	{
		SpotLightBufferSRV->Release();
		SpotLightBufferSRV = nullptr;
	}
}

void FLightManager::UpdateLightBuffer(D3D11RHI* RHIDevice)
{
	if (bHaveToUpdate)
	{
		FLightBufferType LightBuffer{};

		if (AmbientLightList.Num() > 0)
		{
			LightBuffer.AmbientLight = AmbientLightList[0]->GetLightInfo();
		}
		if (DIrectionalLightList.Num() > 0)
		{
			LightBuffer.DirectionalLight = DIrectionalLightList[0]->GetLightInfo();
		}


		if (bPointLightDirty)
		{
			PointLightInfoList.clear();
			for (int Index = 0; Index < PointLightList.Num(); Index++)
			{
				PointLightInfoList.Add(PointLightList[Index]->GetLightInfo());
			}
			RHIDevice->UpdateStructuredBuffer(PointLightBuffer, PointLightInfoList.data(), PointLightNum * sizeof(FPointLightInfo));
			bPointLightDirty = false;
		}
		if (bSpotLightDirty)
		{
			SpotLightInfoList.clear();
			for (int Index = 0; Index < SpotLightList.Num(); Index++)
			{
				SpotLightInfoList.Add(SpotLightList[Index]->GetLightInfo());
			}
			RHIDevice->UpdateStructuredBuffer(SpotLightBuffer, SpotLightInfoList.data(), SpotLightNum * sizeof(FPointLightInfo));
			bSpotLightDirty = false;
		}

		LightBuffer.PointLightCount = PointLightNum;
		LightBuffer.SpotLightCount = SpotLightNum;

		//슬롯 재활용 하고싶을 시 if문 밖으로 빼서 매 프레임 세팅 해줘야함.
		RHIDevice->SetAndUpdateConstantBuffer(LightBuffer);
		if (!PointLightBuffer)
		{
			Initialize(RHIDevice);
		}
		ID3D11ShaderResourceView* SRVList[2]{ PointLightBufferSRV, SpotLightBufferSRV };
		//Gouraud shader 사용 여부로 아래 세팅 분기도 가능, 일단 둘다 바인딩함
		RHIDevice->GetDeviceContext()->PSSetShaderResources(3, 2, SRVList);
		RHIDevice->GetDeviceContext()->VSSetShaderResources(3, 2, SRVList);

		bHaveToUpdate = false;
	}
	
}

template<>
void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	AmbientLightList.Add(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	DIrectionalLightList.Add(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	PointLightList.Add(LightComponent);
	PointLightInfoList.Add(LightComponent->GetLightInfo());
	PointLightNum++;
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	SpotLightList.Add(LightComponent);
	SpotLightInfoList.Add(LightComponent->GetLightInfo());
	SpotLightNum++;
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}


template<>
void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	AmbientLightList.Remove(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	DIrectionalLightList.Remove(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	PointLightList.Remove(LightComponent);
	PointLightNum--;
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	SpotLightList.Remove(LightComponent);
	SpotLightNum--;
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}


template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}