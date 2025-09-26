#pragma once
#include "ResourceBase.h"
#include "Enums.h"

class UShader;
class UTexture;
class UMaterial : public UResourceBase
{
	DECLARE_CLASS(UMaterial, UResourceBase)
public:
    UMaterial() = default;
    void Load(const FString& InFilePath, ID3D11Device* InDevice);

protected:
    ~UMaterial() override = default;

public:
    void SetShader(UShader* ShaderResource);
    UShader* GetShader();

    void SetTexture(UTexture* TextureResource);
    UTexture* GetTexture();

    void SetMaterialInfo(const FObjMaterialInfo& InMaterialInfo) { MaterialInfo = InMaterialInfo; }
    const FObjMaterialInfo& GetMaterialInfo() const { return MaterialInfo; }

private:
	UShader* Shader = nullptr;
	UTexture* Texture= nullptr;
    FObjMaterialInfo MaterialInfo;
};

