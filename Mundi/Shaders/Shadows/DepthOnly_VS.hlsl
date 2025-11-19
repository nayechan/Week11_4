// b0: ModelBuffer (VS) - ModelBufferType과 정확히 일치 (128 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix; // 64 bytes
    row_major float4x4 WorldInverseTranspose; // 64 bytes - 올바른 노멀 변환을 위함
};

// b1: ViewProjBuffer (VS) - ViewProjBufferType과 일치
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;   // 0행 광원의 월드 좌표 + 스포트라이트 반경
    row_major float4x4 InverseProjectionMatrix; 
};

cbuffer SkinningMatrixBuffer : register(b5)
{
    row_major float4x4 SkinningMatrices[1000];
};

#ifdef USE_SKINNING

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 Color : COLOR;
    uint4 BoneIndices : BONEINDICES;
    float4 BoneWeights : BONEWEIGHTS;
};
#else
// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 Color : COLOR;
};
#endif

// 출력은 오직 클립 공간 위치만 필요
struct VS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
};

VS_OUT mainVS(VS_INPUT Input)
{
    VS_OUT Output = (VS_OUT) 0;
    
    #ifdef USE_SKINNING
    // Skinning 행렬에 비균등 스케일이 없다고 가정할 것임(노말에 역전치가 아닌 일반 행렬을 곱할 것임)
    // 비균등 스케일이 없어도 선형보간하면서 노말의 수직성이 깨짐. 그래도 대부분의 상황에서 정확한 계산보다 압도적으로 빠르고
    // 오차가 거의 눈에 띄지 않음. 정확한 계산을 위해서는 애초에 선형 보간이 아니라 듀얼 쿼터니언 스키닝을 써야하는데
    // 듀얼 쿼터니언(이동+회전 8차원 벡터) 스키닝도 아티팩트가 없는 게 아니고 LBS가(Linear Blend Skinning) 위에서 말했다시피 충분히 효율적이고 정확해서 잘 안 씀.
     
    float3 SkinnedNormal = float3(0,0,0);
    float3 SkinnedTangent = float3(0,0,0);
    float4 SkinnedPosition = float4(0,0,0,0);
    for(int Index = 0; Index < 4; Index++)
    {
        row_major float4x4 SkinningMatrix = SkinningMatrices[Input.BoneIndices[Index]];
        float BoneWeight = Input.BoneWeights[Index];
        SkinnedPosition += mul(float4(Input.Position.xyz, 1), SkinningMatrix) * BoneWeight;
        SkinnedNormal += mul(Input.Normal, (float3x3)SkinningMatrix) * BoneWeight;
        SkinnedTangent += mul(Input.Tangent.xyz, (float3x3)SkinningMatrix) * BoneWeight;
    }
    Input.Position = SkinnedPosition;
    Input.Normal = normalize(SkinnedNormal);
    Input.Tangent.xyz = normalize(SkinnedTangent);
    
#endif
    
    // 모델 좌표 -> 월드 좌표 -> 뷰 좌표 -> 클립 좌표
    float4 WorldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    float4 ViewPos = mul(WorldPos, ViewMatrix);
    Output.Position = mul(ViewPos, ProjectionMatrix);
    Output.WorldPosition = WorldPos.xyz;
    
    return Output;
}