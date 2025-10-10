cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

cbuffer DecalBuffer : register(b6)
{
    row_major float4x4 DecalMatrix;
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float3 normal : NORMAL0;
    float4 color : COLOR; // Input color from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 decalPos : POSITION; // decal projection 공간 좌표(w값 유지)
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    
    float4 decalPos = mul(worldPos, DecalMatrix);
    output.decalPos = decalPos;
    
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);
    
    // Depth Bias 적용 (z를 살짝 앞으로 당김)
    output.position.z -= 0.0001 * output.position.w;
    
    return output;
}