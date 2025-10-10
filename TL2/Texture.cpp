#include "pch.h"
#include "Texture.h"
#include <DDSTextureLoader.h>
#include <Windows.h>

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

// UTF-8 -> UTF-16 (Windows) 안전 변환: 한글/비ASCII 경로 대응
	int needed = ::MultiByteToWideChar(CP_UTF8, 0, InFilePath.c_str(), -1, nullptr, 0);
	std::wstring WFilePath;
	if (needed > 0)
	{
		WFilePath.resize(needed - 1);
		::MultiByteToWideChar(CP_UTF8, 0, InFilePath.c_str(), -1, WFilePath.data(), needed);
	}
	else
	{
		int needA = ::MultiByteToWideChar(CP_ACP, 0, InFilePath.c_str(), -1, nullptr, 0);
		if (needA > 0)
		{
			WFilePath.resize(needA - 1);
			::MultiByteToWideChar(CP_ACP, 0, InFilePath.c_str(), -1, WFilePath.data(), needA);
		}
	}

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

	TextureName = InFilePath;
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
