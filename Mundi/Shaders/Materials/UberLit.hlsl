//================================================================================================
// Filename:      UberLit.hlsl
// Description:   오브젝트 표면 렌더링을 위한 기본 Uber 셰이더.
//================================================================================================

// --- 전역 상수 정의 ---
#define NUM_POINT_LIGHT_MAX 16
#define NUM_SPOT_LIGHT_MAX 16

// --- 조명 정보 구조체 (LightInfo.h와 완전히 일치) ---
struct FAmbientLightInfo
{
    float4 Color;       // FLinearColor
    float Intensity;
    float3 Padding;     // FVector Padding
};

struct FDirectionalLightInfo
{
    float4 Color;       // FLinearColor
    float3 Direction;   // FVector
    float Intensity;
};

struct FPointLightInfo
{
    float4 Color;           // FLinearColor
    float3 Position;        // FVector
    float AttenuationRadius; // float
    float3 Attenuation;     // FVector (constant, linear, quadratic)
    float FalloffExponent;  // float
    float Intensity;
    float3 Padding;         // FVector Padding
};

struct FSpotLightInfo
{
    float4 Color;           // FLinearColor
    float3 Position;        // FVector
    float InnerConeAngle;   // float
    float3 Direction;       // FVector
    float OuterConeAngle;   // float
    float3 Attenuation;     // FVector
    float AttenuationRadius; // float
    float Intensity;
    float3 Padding;         // FVector Padding
};

// --- 상수 버퍼 (Constant Buffers) ---
// Following the existing codebase pattern from ConstantBufferType.h

// b0: ModelBuffer (VS) - Matches ModelBufferType, extended with WorldInverseTranspose
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose; // For normal transformation with non-uniform scale
};

// b1: ViewProjBuffer (VS) - Matches ViewProjBufferType
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
};

// b2: CameraBuffer (VS+PS) - Camera properties
cbuffer CameraBuffer : register(b2)
{
    float3 CameraPosition;
    float CameraPadding; // 16-byte alignment
};

// b3: MaterialBuffer (VS+PS) - Material properties
cbuffer MaterialBuffer : register(b3)
{
    float SpecularPower; // Material specular exponent (shininess)
    float3 MaterialPadding; // 16-byte alignment
};

// b8: LightBuffer (VS+PS) - Matches FLightBufferType from ConstantBufferType.h
cbuffer LightBuffer : register(b8)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    FPointLightInfo PointLights[NUM_POINT_LIGHT_MAX];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT_MAX];
    uint PointLightCount;
    uint SpotLightCount;
    float2 Padding; // FVector2D Padding in C++
};

// --- 텍스처 및 샘플러 리소스 ---
Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;     // World position for per-pixel lighting
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

// --- 유틸리티 함수 ---

// Ambient Light Calculation
float3 CalculateAmbientLight(FAmbientLightInfo light, float4 materialColor)
{
    return light.Color.rgb * light.Intensity * materialColor.rgb;
}

// Diffuse Light Calculation (Lambert)
float3 CalculateDiffuse(float3 lightDir, float3 normal, float4 lightColor, float intensity, float4 materialColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0f);
    return lightColor.rgb * intensity * materialColor.rgb * NdotL;
}

// Specular Light Calculation (Blinn-Phong)
// Note: Does not multiply by materialColor - specular is treated as light color only
// If you want material-based specular, pass specular color as a parameter
float3 CalculateSpecular(float3 lightDir, float3 normal, float3 viewDir, float4 lightColor, float intensity, float specularPower)
{
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfVec), 0.0f);
    float specular = pow(NdotH, specularPower);
    return lightColor.rgb * intensity * specular;
}

// Attenuation Calculation for Point/Spot Lights
float CalculateAttenuation(float3 attenuation, float distance)
{
    return 1.0f / (attenuation.x + attenuation.y * distance + attenuation.z * distance * distance);
}

// Attenuation with Falloff Exponent (for Point Lights)
float CalculateAttenuationWithFalloff(float3 attenuation, float distance, float falloffExponent)
{
    float baseAttenuation = CalculateAttenuation(attenuation, distance);
    // Apply falloff exponent to create steeper or gentler falloff curves
    // falloffExponent = 1.0 means linear, > 1.0 means sharper falloff, < 1.0 means gentler
    return pow(baseAttenuation, falloffExponent);
}

// Directional Light Calculation (Diffuse + Specular)
float3 CalculateDirectionalLight(FDirectionalLightInfo light, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightDir = normalize(-light.Direction);

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor);

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower);
    }

    return diffuse + specular;
}

// Point Light Calculation (Diffuse + Specular with Attenuation and Falloff)
float3 CalculatePointLight(FPointLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Early out if beyond radius (using AttenuationRadius from C++ struct)
    if (distance > light.AttenuationRadius)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightDir = lightVec / distance;

    // Use FalloffExponent to control attenuation curve
    float attenuation = CalculateAttenuationWithFalloff(light.Attenuation, distance, light.FalloffExponent);

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower) * attenuation;
    }

    return diffuse + specular;
}

// Spot Light Calculation (Diffuse + Specular with Attenuation and Cone)
float3 CalculateSpotLight(FSpotLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Early out if beyond radius (using AttenuationRadius from C++ struct)
    if (distance > light.AttenuationRadius)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightDir = lightVec / distance;
    float3 spotDir = normalize(light.Direction);

    // Spot cone attenuation
    float cosAngle = dot(-lightDir, spotDir);
    float innerCos = cos(light.InnerConeAngle);
    float outerCos = cos(light.OuterConeAngle);

    // Early out if outside cone
    if (cosAngle < outerCos)
        return float3(0.0f, 0.0f, 0.0f);

    // Distance attenuation using the Attenuation field from C++ struct
    float distanceAttenuation = CalculateAttenuation(light.Attenuation, distance);

    // Smooth falloff between inner and outer cone
    float spotAttenuation = smoothstep(outerCos, innerCos, cosAngle);

    // Combine both attenuations
    float attenuation = distanceAttenuation * spotAttenuation;

    // Diffuse
    float3 diffuse = CalculateDiffuse(lightDir, normal, light.Color, light.Intensity, materialColor) * attenuation;

    // Specular (optional)
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    if (includeSpecular)
    {
        specular = CalculateSpecular(lightDir, normal, viewDir, light.Color, light.Intensity, specularPower) * attenuation;
    }

    return diffuse + specular;
}

//================================================================================================
// 버텍스 셰이더 (Vertex Shader)
//================================================================================================
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Out;

    // Transform position to world space first
    float4 worldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    Out.WorldPos = worldPos.xyz;

    // Then to view space
    float4 viewPos = mul(worldPos, ViewMatrix);

    // Finally to clip space
    Out.Position = mul(viewPos, ProjectionMatrix);

    // Transform normal to world space using inverse transpose for correct non-uniform scale handling
    float3 worldNormal = normalize(mul(Input.Normal, (float3x3) WorldInverseTranspose));
    Out.Normal = worldNormal;

    Out.TexCoord = Input.TexCoord;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Calculate lighting per-vertex (diffuse + specular)
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);

    // Calculate view direction for specular
    float3 viewDir = normalize(CameraPosition - Out.WorldPos);

    // Ambient light
    finalColor += CalculateAmbientLight(AmbientLight, Input.Color);

    // Directional light (diffuse + specular)
    finalColor += CalculateDirectionalLight(DirectionalLight, worldNormal, viewDir, Input.Color, true, SpecularPower);

    // Point lights (diffuse + specular)
    for (int i = 0; i < PointLightCount; i++)
    {
        finalColor += CalculatePointLight(PointLights[i], Out.WorldPos, worldNormal, viewDir, Input.Color, true, SpecularPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < SpotLightCount; j++)
    {
        finalColor += CalculateSpotLight(SpotLights[j], Out.WorldPos, worldNormal, viewDir, Input.Color, true, SpecularPower);
    }

    Out.Color = float4(finalColor, Input.Color.a);

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: Pass data to pixel shader for per-pixel calculation
    Out.Color = Input.Color;

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: Pass data to pixel shader for per-pixel calculation
    Out.Color = Input.Color;

#else
    // No lighting model defined - use vertex color as-is
    Out.Color = Input.Color;

#endif

    return Out;
}

//================================================================================================
// 픽셀 셰이더 (Pixel Shader)
//================================================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    // Sample texture
    float4 texColor = g_DiffuseTexColor.Sample(g_Sample, Input.TexCoord);

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Lighting already calculated in vertex shader
    // Just multiply vertex color with texture
    float4 finalPixel = Input.Color * texColor;
    return finalPixel;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: Calculate diffuse lighting per-pixel (no specular)
    float3 normal = normalize(Input.Normal);
    float4 baseColor = Input.Color * texColor; // Material color with texture
    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light
    litColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional light (diffuse only)
    litColor += CalculateDirectionalLight(DirectionalLight, normal, float3(0, 0, 0), baseColor, false, 0.0f);

    // Point lights (diffuse only)
    for (int i = 0; i < PointLightCount; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
    }

    // Spot lights (diffuse only)
    for (int j = 0; j < SpotLightCount; j++)
    {
        litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
    }

    // Preserve original alpha (lighting doesn't affect transparency)
    return float4(litColor, baseColor.a);

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: Calculate diffuse and specular lighting per-pixel (Blinn-Phong)
    float3 normal = normalize(Input.Normal);
    float3 viewDir = normalize(CameraPosition - Input.WorldPos); // View direction from camera to fragment
    float4 baseColor = Input.Color * texColor; // Material color with texture
    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light
    litColor += CalculateAmbientLight(AmbientLight, baseColor);

    // Directional light (diffuse + specular)
    litColor += CalculateDirectionalLight(DirectionalLight, normal, viewDir, baseColor, true, SpecularPower);

    // Point lights (diffuse + specular)
    for (int i = 0; i < PointLightCount; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, viewDir, baseColor, true, SpecularPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < SpotLightCount; j++)
    {
        litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, viewDir, baseColor, true, SpecularPower);
    }

    // Preserve original alpha (lighting doesn't affect transparency)
    return float4(litColor, baseColor.a);

#else
    // No lighting model defined - use texture color with vertex color
    float4 finalPixel = Input.Color * texColor;
    return finalPixel;

#endif
}