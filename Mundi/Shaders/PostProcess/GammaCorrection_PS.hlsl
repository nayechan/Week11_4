Texture2D g_SceneColorTex : register(t0);
SamplerState g_LinearClampSample : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;  
}; 
 

cbuffer GammaCB : register(b2)
{
    float Gamma;
    float3 Padding;
}
 
float3 GammaCorrection(float4 Color)
{
    
    float invGamma = 1.0 / Gamma;
     
    
    return pow(Color.rgb, invGamma);
}

float4 mainPS(PS_INPUT input) : SV_Target
{  
    float4 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);
    
    // 0 이 중앙, 1이 가장자리
    
    return float4(GammaCorrection(SceneColor), SceneColor.a);
}