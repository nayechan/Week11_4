Texture2D g_DepthTex : register(t0);
Texture2D g_PrePathResultTex : register(t1);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer InvViewProjBuffer : register(b1)
{
    row_major float4x4 InvViewMatrix;
    row_major float4x4 InvProjectionMatrix;
}

cbuffer FogCB : register(b2)
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    
    float4 FogInscatteringColor;
    
    float FogMaxOpacity;
    float FogHeight; // fog base height
    
}

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = g_PrePathResultTex.Sample(g_LinearClampSample, input.texCoord);
    float depth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    // -----------------------------
    // 1. Depth → Clip Space 복원
    // -----------------------------
    // DX11에서는 depth가 [0,1] 범위 → NDC z 그대로 사용 가능
    //float ndcZ = depth * 2.0f - 1.0f; // clip space 변환

    // NDC 좌표
    float ndcX = input.texCoord.x * 2.0f - 1.0f;
    float ndcY = 1.0f - input.texCoord.y * 2.0f; // y축 반전
    float4 ndcPos = float4(ndcX, ndcY, depth, 1.0f);

    // -----------------------------
    // 2. 월드 좌표 복원
    // -----------------------------
    float4 viewPos = mul(ndcPos, InvProjectionMatrix);
    viewPos /= viewPos.w;
    float4 worldPos = mul(viewPos, InvViewMatrix);
    worldPos /= worldPos.w;
    
    // 카메라 월드 좌표
    float4 cameraWorldPos = mul(float4(0, 0, 0, 1), InvViewMatrix);

    // -----------------------------
    // 3. 광선 벡터 및 거리
    // -----------------------------
    float3 ray = worldPos.xyz - cameraWorldPos.xyz;
    float L = length(ray);
    float3 rayDir = ray / L;

    //////////////////////////////////

    //// -----------------------------
    //// 4. Exponential Height Fog 적분 (z축 기준)
    //// -----------------------------
    //float zc = cameraWorldPos.z - FogHeight;
    //float dz = rayDir.z;

    //float opticalDepth = 0.0f;

    //// f = GlobalDensity * exp(-HeightFalloff * (특정 픽셀 높이 - FogHeight)) 함수에 대한 적분 [0, 해당 픽셀까지 거리]
    //if (abs(dz) > 1e-5)
    //{
    //    float termC = exp(-FogHeightFalloff * zc);
    //    float termP = exp(-FogHeightFalloff * (zc + dz * L));

    //    opticalDepth = FogDensity * (termC - termP) / (FogHeightFalloff * dz);
    //}
    //else
    //{
    //     // dz ≈ 0 → Taylor 전개 근사
    //    // exp(-h(zc + dz*L)) ≈ exp(-h*zc) * (1 - h*dz*L + 0.5*(h*dz*L)^2 ...)
    //    float expTerm = exp(-FogHeightFalloff * zc);
    //    float hL = FogHeightFalloff * L;

    //    // 1차 근사: FogDensity * exp(-h*zc) * L
    //    opticalDepth = FogDensity * expTerm * (L - 0.5f * dz * hL * L);
    //}

    //// -----------------------------
    //// 5. Transmittance & Fog Factor
    //// -----------------------------
    //float transmittance = exp(-opticalDepth);
    //float fogFactor = 1.0 - transmittance;

    //// 시작 거리와 컷오프 적용
    //float distFactor = saturate((L - StartDistance) / (FogCutoffDistance - StartDistance));
    //fogFactor *= distFactor;

    //// 최대 불투명도 적용
    //fogFactor = saturate(fogFactor * FogMaxOpacity);

    
    /////////////////////////////////
    
    //float Distance = clamp(L - StartDistance, 0.0, FogCutoffDistance - StartDistance);
    
    float Distance = max(0.0, L - StartDistance);
    if(Distance > (FogCutoffDistance - StartDistance))
        Distance = 0.0;
    
    float oy = cameraWorldPos.z;
    float dy = rayDir.z;

    float baseExp = exp(-FogHeightFalloff * (oy - FogHeight));

    float FogIntegral = 0.0;
    if (abs(dy) > 1e-6) {
        // d_y ≠ 0 인 경우
        float numerator = 1.0 - exp(-FogHeightFalloff * dy * Distance);
        float denominator = FogHeightFalloff * dy;
        FogIntegral = FogDensity * baseExp * (numerator / denominator);
    } else {
        // d_y = 0 (수평 광선)
        FogIntegral = FogDensity * baseExp * Distance;
    }
    
    float fogFactor = exp(-FogIntegral);
    // 최대 불투명도 적용
    fogFactor = clamp(fogFactor, 1 - FogMaxOpacity, 1.0);
    
    // 최종 색상
    //color.rgb = lerp(color.rgb, FogInscatteringColor.rgb, fogFactor);
    color.rgb = color.rgb * fogFactor + FogInscatteringColor.rgb * (1 - fogFactor);

    return color;
}
