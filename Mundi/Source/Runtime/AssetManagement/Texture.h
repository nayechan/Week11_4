#pragma once
#include "ResourceBase.h"
#include <d3d11.h>

class UTexture : public UResourceBase
{
public:
	DECLARE_CLASS(UTexture, UResourceBase)

	UTexture();
	virtual ~UTexture();

	// 실제 로드된 파일 경로를 반환 (DDS 캐시 사용 시 DDS 경로 반환)
	// bSRGB: true = sRGB 포맷 사용 (Diffuse/Albedo 텍스처), false = Linear 포맷 (Normal/Data 텍스처)
	FString Load(const FString& InFilePath, ID3D11Device* InDevice, bool bSRGB = true);

	ID3D11ShaderResourceView* GetShaderResourceView() const { return ShaderResourceView; }
	ID3D11Texture2D* GetTexture2D() const { return Texture2D; }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	DXGI_FORMAT GetFormat() const { return Format; }
	void SetTextureName(const FString& InTextureName) { TextureName = InTextureName; }
	const FString& GetTextureName() { return  TextureName; }

	// 원본 소스 파일 경로 (DDS 캐시가 아닌 원본 경로)
	void SetSourceFilePath(const FString& InSourcePath) { SourceFilePath = InSourcePath; }
	const FString& GetSourceFilePath() const { return SourceFilePath.empty() ? TextureName : SourceFilePath; }

	void ReleaseResources();

private:
	FString TextureName;
	FString SourceFilePath;  // 원본 소스 경로 (예: Data/cube_texture.png)
	ID3D11Texture2D* Texture2D;
	ID3D11ShaderResourceView* ShaderResourceView;

	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};
