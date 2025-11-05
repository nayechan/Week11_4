Texture2D    g_SceneColorTex   : register(t0);

SamplerState g_LinearClampSample  : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer FadeCB : register(b2)
{
    float4 FadeColor;    // rgb: 색, a: 사용 안함
    float  Opacity;      // 0~1 (시간에 따라 변화)
    float  Weight;       // 0~1 (모디파이어 가중치)
    float2 _Pad;         // 정렬용
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 Color = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);
    
    float K = saturate(Opacity * Weight);
    float3 OutColor = lerp(Color.rgb, FadeColor.rgb, K);

    return float4(OutColor, Color.a);
}