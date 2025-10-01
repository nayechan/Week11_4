// Generic textured billboard shader
cbuffer CameraInfo : register(b0)
{
    float3 textWorldPos;
    row_major matrix viewMatrix;
    row_major matrix projectionMatrix;
    row_major matrix viewInverse;
};

struct VS_INPUT
{
    float3 centerPos : WORLDPOSITION;
    float2 size      : SIZE;
    float4 uvRect    : UVRECT;     // we use xy as per-vertex UV
    uint   vertexId  : SV_VertexID; // not used, but kept for parity
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

Texture2D BillboardTex : register(t0);
SamplerState LinearSamp : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT o;
    // Align quad corners with camera (ignore camera rotation)
    float3 posAligned = mul(float4(input.centerPos, 0.0f), viewInverse).xyz;
    float3 worldPos   = textWorldPos + posAligned;

    o.pos = mul(float4(worldPos, 1.0f), mul(viewMatrix, projectionMatrix));
    o.uv  = input.uvRect.xy;
    return o;
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    float4 c = BillboardTex.Sample(LinearSamp, i.uv);
    // Optional alpha clip for cutout textures
    // clip(c.a - 0.05f);
    return c;
}
