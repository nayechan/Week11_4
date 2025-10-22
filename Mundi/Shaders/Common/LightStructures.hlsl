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

struct FAmbientLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
};

struct FDirectionalLightInfo
{
    float4 Color;       // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Direction;   // 12 bytes - FVector
    float Padding;      // 4 bytes - 정렬을 위한 패딩
};

struct FPointLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Position;        // 12 bytes - FVector
    float AttenuationRadius; // 4 bytes (슬롯 채우기 위해 위로 이동)
    float FalloffExponent;  // 4 bytes - 예술적 제어를 위한 감쇠 지수
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = 물리 기반, false = 지수 기반)
    float2 Padding;         // 8 bytes - 정렬을 위한 패딩 (Attenuation 제거됨)
};

struct FSpotLightInfo
{
    float4 Color;           // 16 bytes - FLinearColor (Intensity + Temperature 포함)
    float3 Position;        // 12 bytes - FVector
    float InnerConeAngle;   // 4 bytes - 내부 원뿔 각도
    float3 Direction;       // 12 bytes - FVector
    float OuterConeAngle;   // 4 bytes - 외부 원뿔 각도
    float AttenuationRadius; // 4 bytes - 감쇠 반경
    float FalloffExponent;  // 4 bytes - 예술적 제어를 위한 감쇠 지수
    uint bUseInverseSquareFalloff; // 4 bytes - uint32 (true = 물리 기반, false = 지수 기반)
    float Padding;         // 4 bytes - 정렬을 위한 패딩 (Attenuation 제거됨)
};
