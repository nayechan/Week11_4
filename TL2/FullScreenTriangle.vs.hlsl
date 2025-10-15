// 화면을 가득 채우는 삼각형을 만드는 VS
// DeviceContext->Draw(3, 0); 으로 호출해야 됨

// 셰이더의 출력 구조체 정의
struct VS_OUTPUT
{
    float4 Position : SV_POSITION; // 최종 클립 공간 좌표
    float2 TexCoord : TEXCOORD0; // 텍스처를 샘플링할 UV 좌표
};

// 정점 버퍼 입력이 없으므로, main 함수의 파라미터로 SV_VertexID만 받습니다.
VS_OUTPUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Out;

    // VertexID (0, 1, 2)를 기반으로 UV 좌표를 계산합니다.
    // ID 0 -> (0, 0)
    // ID 1 -> (2, 0)
    // ID 2 -> (0, 2)
    Out.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);

    // UV 좌표를 클립 공간 좌표로 변환합니다.
    // TexCoord (0,0) -> Position (-1, 1)  : 왼쪽 위
    // TexCoord (2,0) -> Position ( 3, 1)  : 오른쪽 저 멀리
    // TexCoord (0,2) -> Position (-1,-3)  : 아래쪽 저 멀리
    Out.Position = float4(Out.TexCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return Out;
}