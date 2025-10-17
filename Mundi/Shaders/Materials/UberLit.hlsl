//================================================================================================
// Filename:      UberLit.hlsl
// Description:   오브젝트 표면 렌더링을 위한 기본 Uber 셰이더.
//================================================================================================

// --- 전역 상수 정의 ---
#define NUM_POINT_LIGHT 4
#define NUM_SPOT_LIGHT 4

// --- 조명 정보 구조체 ---
struct FAmbientLightInfo
{
    float4 Color;       // FLinearColor (RGBA)
    float Intensity;
    float Padding[3];   // 16-byte alignment
};

struct FDirectionalLightInfo
{
    float3 Direction;   // FVector
    float Intensity;
    float4 Color;       // FLinearColor (RGBA)
};

struct FPointLightInfo
{
    float3 Position;    // FVector
    float Radius;
    float3 Attenuation; // FVector (constant, linear, quadratic)
    float Intensity;
    float4 Color;       // FLinearColor (RGBA)
};

struct FSpotLightInfo
{
    float3 Position;    // FVector
    float InnerConeAngle;
    float3 Direction;   // FVector
    float OuterConeAngle;
    float4 Color;       // FLinearColor (RGBA)
    float Intensity;
    float Radius;
    float Padding[2];   // 16-byte alignment
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
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
    uint DirectionalLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    uint LightPadding;
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
float3 CalculateAmbientLight(FAmbientLightInfo info, float4 materialColor)
{
    return info.Color.rgb * info.Intensity * materialColor.rgb;
}

// Diffuse Light Calculation (Lambert)
float3 CalculateDiffuse(float3 lightDir, float3 normal, float4 lightColor, float intensity, float4 materialColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0f);
    return lightColor.rgb * intensity * materialColor.rgb * NdotL;
}

// Specular Light Calculation (Blinn-Phong)
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

// Point Light Calculation (Diffuse + Specular with Attenuation)
float3 CalculatePointLight(FPointLightInfo light, float3 worldPos, float3 normal, float3 viewDir, float4 materialColor, bool includeSpecular, float specularPower)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);

    // Early out if beyond radius
    if (distance > light.Radius)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightDir = lightVec / distance;
    float attenuation = CalculateAttenuation(light.Attenuation, distance);

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

    // Early out if beyond radius
    if (distance > light.Radius)
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

    // NOTE: FSpotLightInfo does NOT have Attenuation field in the C++ struct
    // Using simple linear falloff based on radius instead
    // If you want quadratic attenuation, add Attenuation field to FSpotLightInfo C++ struct
    float distanceAttenuation = 1.0f - saturate(distance / light.Radius);

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
    for (int i = 0; i < NUM_POINT_LIGHT; i++)
    {
        finalColor += CalculatePointLight(PointLights[i], Out.WorldPos, worldNormal, viewDir, Input.Color, true, SpecularPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < NUM_SPOT_LIGHT; j++)
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
    for (int i = 0; i < NUM_POINT_LIGHT; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f);
    }

    // Spot lights (diffuse only)
    for (int j = 0; j < NUM_SPOT_LIGHT; j++)
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
    for (int i = 0; i < NUM_POINT_LIGHT; i++)
    {
        litColor += CalculatePointLight(PointLights[i], Input.WorldPos, normal, viewDir, baseColor, true, SpecularPower);
    }

    // Spot lights (diffuse + specular)
    for (int j = 0; j < NUM_SPOT_LIGHT; j++)
    {
        litColor += CalculateSpotLight(SpotLights[j], Input.WorldPos, normal, viewDir, baseColor, true, SpecularPower);
    }

    // Preserve original alpha (lighting doesn't affect transparency)
    return float4(litColor, baseColor.a);

#endif
    
    return float4(1.0f, 0.0f, 1.0f, 1.0f); // Magenta for error indication
}