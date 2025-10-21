#pragma once

class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class ULightComponent;
class D3D11RHI;

enum class ELightType
{
    AmbientLight,
    DirectionalLight,
    PointLight,
    SpotLight,
};
struct FAmbientLightInfo
{
    FLinearColor Color;     // 16 bytes - Color already includes Intensity and Temperature
    // Total: 16 bytes
};

struct FDirectionalLightInfo
{
    FLinearColor Color;     // 16 bytes - Color already includes Intensity and Temperature
    FVector Direction;      // 12 bytes
    float Padding;          // 4 bytes padding to reach 32 bytes (16-byte aligned)
    // Total: 32 bytes
};

struct FPointLightInfo
{
    FLinearColor Color;         // 16 bytes - Color already includes Intensity and Temperature
    FVector Position;           // 12 bytes
    float AttenuationRadius;    // 4 bytes (moved up to fill slot)
    float FalloffExponent;      // 4 bytes - Falloff exponent for artistic control
    uint32 bUseInverseSquareFalloff; // 4 bytes - true = physically accurate, false = exponent-based
	FVector2D Padding;            // 8 bytes padding to reach 48 bytes (16-byte aligned)
	// Total: 48 bytes
};

struct FSpotLightInfo
{
    FLinearColor Color;         // 16 bytes - Color already includes Intensity and Temperature
    FVector Position;           // 12 bytes
    float InnerConeAngle;       // 4 bytes
    FVector Direction;          // 12 bytes
    float OuterConeAngle;       // 4 bytes
    float AttenuationRadius;    // 4 bytes
    float FalloffExponent;      // 4 bytes - Falloff exponent for artistic control
    uint32 bUseInverseSquareFalloff; // 4 bytes - true = physically accurate, false = exponent-based
	float Padding;            // 4 bytes padding to reach 64 bytes (16-byte aligned)
	// Total: 64 bytes
};

class FLightManager
{

public:
    FLightManager() = default;
    ~FLightManager();

    void Initialize(D3D11RHI* RHIDevice);
    void Release();


    void UpdateLightBuffer(D3D11RHI* RHIDevice);
    void SetDirtyFlag();

    TArray<FPointLightInfo>& GetPointLightInfoList() { return PointLightInfoList; }
    TArray<FSpotLightInfo>& GetSpotLightInfoList() { return SpotLightInfoList; }

    template<typename T>
    void RegisterLight(T* LightComponent);
    template<typename T>
    void DeRegisterLight(T* LightComponent);
    template<typename T>
    void UpdateLight(T* LightComponent);
private:

    bool bHaveToUpdate = true;
    bool bPointLightDirty = true;
    bool bSpotLightDirty = true;

    //structured buffer
    ID3D11Buffer* PointLightBuffer = nullptr;
    ID3D11Buffer* SpotLightBuffer = nullptr;
    ID3D11ShaderResourceView* PointLightBufferSRV = nullptr;
    ID3D11ShaderResourceView* SpotLightBufferSRV = nullptr;


    TArray<UAmbientLightComponent*> AmbientLightList;
    TArray<UDirectionalLightComponent*> DIrectionalLightList;
    TArray<UPointLightComponent*> PointLightList;
    TArray<USpotLightComponent*> SpotLightList;

    //업데이트 시에만 clear하고 다시 수집할 실제 데이터 리스트
    TArray<FPointLightInfo> PointLightInfoList;
    TArray<FSpotLightInfo> SpotLightInfoList;

    //이미 레지스터된 라이트인지 확인하는 용도
    TSet<ULightComponent*> LightComponentList;
    uint32 PointLightNum = 0;
    uint32 SpotLightNum = 0;
};

template<> void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent);