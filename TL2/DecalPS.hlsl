Texture2D g_DecalTexColor : register(t0);
SamplerState g_Sample : register(s0);

cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float UVScrollTime;
    float _pad_scrollcb;
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 decalPos : POSITION; // decal projection 공간 좌표(w값 유지)
};

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 ndc = input.decalPos.xyz / input.decalPos.w;
    
    // decal의 forward가 +x임 -> x방향 projection
    if(ndc.x < 0.0f || 1.0f < ndc.x || ndc.y < -1.0f || 1.0f < ndc.y || ndc.z < -1.0f || 1.0f < ndc.z)
    {
        discard;
    }
    
    // for test
    //return float4(1.0, 0.0f, 0.0f, 1.0f);
    
    // ndc to uv
    float2 uv = (ndc.yz + 1.0f) / 2.0f;
    uv.y *= -1.0f;
    
    uv += UVScrollSpeed * UVScrollTime;
    
    float4 finalColor = g_DecalTexColor.Sample(g_Sample, uv);
    
    return finalColor;
}

