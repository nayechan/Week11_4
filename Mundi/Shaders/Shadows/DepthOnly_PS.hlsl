// b1: ViewProjBuffer (VS) - ViewProjBufferType과 일치
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;   // 0행 광원의 월드 좌표 + 스포트라이트 반경
    row_major float4x4 InverseProjectionMatrix;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
};

float2 mainPS(PS_INPUT Input) : SV_TARGET
{
    float3 LightWorldPosition = float3(InverseViewMatrix[0][0], InverseViewMatrix[0][1], InverseViewMatrix[0][2]);
    float LightRadius = InverseViewMatrix[0][3];
    float Distance = length(Input.WorldPosition - LightWorldPosition);
    float NormalizedDepth = saturate(Distance / LightRadius);
    float Moment1 = NormalizedDepth;
    float Moment2 = NormalizedDepth * NormalizedDepth;

    return float2(Moment1, Moment2);
}