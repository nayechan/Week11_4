//================================================================================================
// Filename:      LightStructures.hlsl
// Description:   조명 구조체 정의
//                모든 조명 기반 셰이더에서 공유 (UberLit, Decal 등)
//================================================================================================

// --- 전역 상수 정의 ---
#define NUM_POINT_LIGHT_MAX 16
#define NUM_SPOT_LIGHT_MAX 16

// --- 조명 정보 구조체 (LightInfo.h와 완전히 일치) ---
// 주의: Color는 이미 Intensity와 Temperature가 포함됨 (C++에서 계산)
// 최소 메모리 사용을 위한 최적화된 패딩

struct FShadowMapData
{
    row_major float4x4 ShadowViewProjMatrix;
    float4 AtlasScaleOffset;
    int SampleCount;
    float3 Padding;
};

struct FAmbientLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
};

struct FDirectionalLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Direction;   // 12 bytes - FVector
    float bCastShadows;
    
    uint CascadeCount; // 4 bytes
    float3 Padding;
    
    FShadowMapData Cascades[4];
};

struct FPointLightInfo
{
    float4 Color; // 16 bytes
    float3 Position; // 12 bytes
    uint bCastShadows; // 4 bytes (0 or 1)

    float AttenuationRadius; // 4 bytes
    float FalloffExponent; // 4 bytes
    uint bUseInverseSquareFalloff; // 4 bytes
    int ShadowArrayIndex; // 4 bytes (t8 TextureCubeArray의 슬라이스 인덱스, -1=섀도우 없음)
};

struct FSpotLightInfo
{
    float4 Color; // 16 bytes
    float3 Position; // 12 bytes
    uint bCastShadows; // 4 bytes (0 or 1)

    float3 Direction; // 12 bytes
    float InnerConeAngle; // 4 bytes
    float OuterConeAngle; // 4 bytes
    float AttenuationRadius; // 4 bytes

    float FalloffExponent; // 4 bytes
    uint bUseInverseSquareFalloff; // 4 bytes

    FShadowMapData ShadowData; // 80 bytes (FMatrix(64) + FVector4(16))
};
