#pragma once
constexpr uint32 NUM_POINT_LIGHT_MAX = 16;
constexpr uint32 NUM_SPOT_LIGHT_MAX = 16;

struct FAmbientLightInfo
{
    FLinearColor Color;     // Color already includes Intensity and Temperature

    float Padding0;         // Padding for 16-byte alignment
    FVector Padding;
};

struct FDirectionalLightInfo
{
    FLinearColor Color;     // Color already includes Intensity and Temperature

    float Padding0;         // Padding for 16-byte alignment
    FVector Direction;
};

struct FPointLightInfo
{
    FLinearColor Color;     // Color already includes Intensity and Temperature

    FVector Position;
    float FalloffExponent;

    FVector Attenuation;    // 상수, 일차항, 이차항
    float AttenuationRadius;

    uint32 bUseAttenuationCoefficients;
    FVector Padding;        // 12 bytes padding for 16-byte alignment
};

struct FSpotLightInfo
{
    FLinearColor Color;     // Color already includes Intensity and Temperature

    FVector Position;
    float InnerConeAngle;

    FVector Direction;
    float OuterConeAngle;

    FVector Attenuation;
    float AttenuationRadius;

    float FalloffExponent;
    uint32 bUseAttenuationCoefficients;
    FVector2D Padding;      // 8 bytes padding for 16-byte alignment
};