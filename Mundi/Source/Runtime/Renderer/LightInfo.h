#pragma once
constexpr uint32 NUM_POINT_LIGHT_MAX = 16;
constexpr uint32 NUM_SPOT_LIGHT_MAX = 16;

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
    FVector Attenuation;        // 12 bytes (constant, linear, quadratic)
    float FalloffExponent;      // 4 bytes
    uint32 bUseAttenuationCoefficients; // 4 bytes
    FVector Padding;              // 12 bytes padding to reach 64 bytes (16-byte aligned)
	// Total: 64 bytes
};

struct FSpotLightInfo
{
    FLinearColor Color;         // 16 bytes - Color already includes Intensity and Temperature
    FVector Position;           // 12 bytes
    float InnerConeAngle;       // 4 bytes
    FVector Direction;          // 12 bytes
    float OuterConeAngle;       // 4 bytes
    FVector Attenuation;        // 12 bytes
    float AttenuationRadius;    // 4 bytes
    float FalloffExponent;      // 4 bytes
    uint32 bUseAttenuationCoefficients; // 4 bytes
	FVector2D Padding;          // 8 bytes padding to reach 80 bytes (16-byte aligned)
	// Total: 80 bytes
};