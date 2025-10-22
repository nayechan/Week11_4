//================================================================================================
// Filename:      LightingBuffers.hlsl
// Description:   조명 상수 버퍼 및 리소스
//                모든 조명 기반 셰이더에서 공유
//================================================================================================

// 주의: 이 파일은 LightStructures.hlsl 이후에 include 되어야 함

// b7: CameraBuffer (VS+PS) - 카메라 속성
cbuffer CameraBuffer : register(b7)
{
    float3 CameraPosition;
};

// b8: LightBuffer (VS+PS) - ConstantBufferType.h의 FLightBufferType과 일치
cbuffer LightBuffer : register(b8)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    uint PointLightCount;
    uint SpotLightCount;
};

// --- 타일 기반 라이트 컬링 리소스 ---
// t2: 타일별 라이트 인덱스 Structured Buffer
// 구조:  [TileIndex * MaxLightsPerTile] = LightCount
//        [TileIndex * MaxLightsPerTile + 1 ~ ...] = LightIndices (상위 16비트: 타입, 하위 16비트: 인덱스)
StructuredBuffer<uint> g_TileLightIndices : register(t2);

// PointLight, SpotLight Structured Buffer
StructuredBuffer<FPointLightInfo> g_PointLightList : register(t3);
StructuredBuffer<FSpotLightInfo> g_SpotLightList : register(t4);

// b11: 타일 컬링 설정 상수 버퍼
cbuffer TileCullingBuffer : register(b11)
{
    uint TileSize;          // 타일 크기 (픽셀, 기본 16)
    uint TileCountX;        // 가로 타일 개수
    uint TileCountY;        // 세로 타일 개수
    uint bUseTileCulling;   // 타일 컬링 활성화 여부 (0=비활성화, 1=활성화)
};
