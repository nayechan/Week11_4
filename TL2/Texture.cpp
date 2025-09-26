#include "pch.h"
#include "Texture.h"
#include <DDSTextureLoader.h>

UTexture::UTexture()
{
	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}

UTexture::~UTexture()
{
	ReleaseResources();
}

void UTexture::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	assert(InDevice);

	std::wstring WFilePath;
	WFilePath = std::wstring(InFilePath.begin(), InFilePath.end());

	HRESULT hr = DirectX::CreateDDSTextureFromFile(
		InDevice,
		WFilePath.c_str(),
		reinterpret_cast<ID3D11Resource**>(&Texture2D),
		&ShaderResourceView
	);
	if (FAILED(hr))
	{
		UE_LOG("!!!LOAD TEXTIRE FAILED!!!");
	}

	if (Texture2D)
	{
		D3D11_TEXTURE2D_DESC desc;
		Texture2D->GetDesc(&desc);
		Width = desc.Width;
		Height = desc.Height;
		Format = desc.Format;
	}

	UE_LOG("Successfully loaded DDS texture: %s", InFilePath);
}

void UTexture::ReleaseResources()
{
	if(Texture2D)
	{
		Texture2D->Release();
	}

	if(ShaderResourceView)
	{
		ShaderResourceView->Release();
	}

	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}
