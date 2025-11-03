//================================================================================================
// Filename:      LightStructures.hlsl
// Description:   조명 구조체 정의
//                모든 조명 기반 셰이더에서 공유 (UberLit, Decal 등)
//================================================================================================

#define CASCADED_MAX 8

// --- 조명 정보 구조체 (LightInfo.h와 완전히 일치) ---
// 주의: Color는 이미 Intensity와 Temperature가 포함됨 (C++에서 계산)
// 최소 메모리 사용을 위한 최적화된 패딩

struct FShadowMapData
{
    row_major float4x4 ShadowViewProjMatrix;
    float4 AtlasScaleOffset;
    float3 WorldPos;
    int SampleCount;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float Padding;
};

struct FAmbientLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
};

struct FDirectionalLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Direction;   // 12 bytes - FVector
    uint bCastShadows;
    
    uint bCascaded;
    uint CascadeCount; // 4 bytes
    float CascadedOverlapValue;
    float CascadedAreaColorDebugValue;
    
    float CascadedAreaShadowDebugValue;
    float3 DirectionalLightInfoPadding;
    
    float4 CascadedSliceDepth[CASCADED_MAX / 4 + 1];

    FShadowMapData Cascades[CASCADED_MAX];
};

struct FPointLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Position;        // 12 bytes - FVector
    float AttenuationRadius; // 4 bytes (슬롯 채우기 위해 위로 이동)
    float FalloffExponent;  // 4 bytes - 예술적 제어를 위한 감쇠 지수
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = 물리 기반, false = 지수 기반)
    uint bCastShadows;      // 4 bytes - 쉐도우 캐스팅 여부
    uint LightIndex;        // 4 bytes - 쉐도우 맵 배열 인덱스
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float Padding;
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
