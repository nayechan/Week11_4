// Model/View/Projection buffers (match other material shaders)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose;
};

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// b3: ColorBuffer (PS) - 색상 블렌딩/lerp용
cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor; // 블렌드할 색상 (알파가 블렌드 양 제어)
    uint UUID;
}; 

cbuffer FireballCB : register(b6)
{
    float Time;
    float2 Resolution; // 선택
    float _pad0;

    float3 CameraPosition; // ViewDir 계산용
    float _pad1;

    float2 UVScrollSpeed; // 노이즈 스크롤
    float2 UVRotateRad; // 노이즈 회전 각도(라디안)

    uint LayerCount; // 8~12 권장
    float LayerDivBase; // ShaderToy의 1/i 스케일: 보통 1.0
    float GateK; // step(1 - i/6, alpha)의 6 상수
    float IntensityScale; // 최종 강도 스케일
}

Texture2D g_NoiseTex : register(t0);
SamplerState g_Samp : register(s1);

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
};
struct VS_OUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL0;
    float2 UV : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

VS_OUT FireballVS(VS_IN In)
{
    VS_OUT Out;

    float4 worldPos = mul(float4(In.Position, 1), WorldMatrix);
    Out.WorldPos = worldPos.xyz;

    // 클립
    float4 viewPos = mul(worldPos, ViewMatrix);
    Out.Position = mul(viewPos, ProjectionMatrix);

    // 노멀
    Out.Normal = normalize(mul(In.Normal, (float3x3) WorldInverseTranspose));

    // 카메라→뷰 방향
    Out.ViewDir = normalize(CameraPosition - Out.WorldPos);

    // UV가 있으면 사용. 없으면 구형 매핑
    float2 uv = In.TexCoord;
    if (all(uv == float2(0.0, 0.0)))
    {
        float3 n = normalize(Out.Normal);
        float u = atan2(n.z, n.x) / (2.0 * 3.14159265) + 0.5;
        float v = n.y * 0.5 + 0.5;
        uv = float2(u, v);
    }
    Out.UV = uv;
    return Out;
}


struct PS_OUT
{
    float4 Color : SV_Target0;
    uint UUID : SV_Target1;
};

float3 FireColor(float x)
{
    float3 c = 0;
    c += float3(1, 0, 0) * x;
    c += float3(1, 1, 0) * saturate(x - 0.5);
    c += float3(1, 1, 1) * saturate(x - 0.7);
    return c;
}

float2 rot2(float2 p, float ang)
{
    float s = sin(ang), c = cos(ang);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// 3D axis-angle rotation to emulate GLSL tex/wind matrices
float3 rotate_axis_angle(float3 v, float3 axis, float ang)
{
    float s = sin(ang), c = cos(ang);
    float3 a = normalize(axis);
    return v * c + cross(a, v) * s + a * dot(a, v) * (1.0 - c);
}
float get_layer_scale(float layer_index)
{
    return 1.0 - layer_index / 40.0;
}
 
// New implementation matching GLSL layering using vertex info
PS_OUT FireballPS(VS_OUT In)
{ 
    PS_OUT o;
    o.UUID = UUID; 
    // Base 2D flow
    float2 uvBase = In.UV + UVScrollSpeed * Time;
    uvBase = rot2(uvBase, UVRotateRad.x);

    // Center/rim weights using view-dependent normal term
    float ndv = saturate(dot(normalize(In.Normal), normalize(In.ViewDir)));
    float centerW = ndv;
    float rimW = pow(1.0 - ndv, 2.0);

    float3 n0 = normalize(In.Normal);
    float intensity = 0.0;
    
    [loop]
    for (uint i = 0; i < LayerCount; ++i)
    {
        float fi = (float)i;
        
        // 레이어별 반지름 스케일 적용 (GLSL의 핵심 부분)
        float layer_scale = get_layer_scale(fi);

        // Emulate GLSL tex_mat and wind_mat rotations per layer
        float3 n = n0;
        n = rotate_axis_angle(n, float3(0.3, -0.7, 0.1), radians(11.0) * Time * (fi + 1.0));
        n = rotate_axis_angle(n, float3(1.0, 0.0, 0.0), radians(25.0) * Time * (fi + 1.0));

        // 레이어 스케일을 노멀에 적용 (구의 반지름 변화 근사)
        n = normalize(n0 + (n - n0) * layer_scale);

        // Normal-to-UV mapping similar to GLSL sampling
        float2 uvN;
        uvN.x = atan2(n.z, n.x) / (3.14159265 * 0.5);
        uvN.y = n.y;

        float div = (fi > 0) ? fi : 1.0;
        float2 uv = uvN / (div * LayerDivBase) + uvBase;
        uv = rot2(uv, UVRotateRad.y * Time * (fi + 1.0));

        float alpha = g_NoiseTex.Sample(g_Samp, uv).r;
        float gate = step(1.0 - (fi + 1.0) / max(GateK, 1.0), alpha);
        
        // 레이어 스케일을 가중치에도 반영
        float w = lerp(centerW, rimW, saturate(fi / max(LayerCount, 1.0))) * layer_scale;
        
        intensity += gate * 0.8 * alpha * w;
    }
    
    o.Color = float4(FireColor(intensity * IntensityScale), 1.0);

    return o;
}

float avg_rgb(float3 rgb)
{
    return (rgb.g + rgb.b + rgb.r) / 3.0;
}
// Entry points expected by the engine
VS_OUT mainVS(VS_IN In) 
{ return FireballVS(In); }
PS_OUT mainPS(VS_OUT In)
{
    PS_OUT output = FireballPS(In);
    
//    if (avg_rgb(output.Color.rgb) < 0.1f) 
//        discard;
   // output.Color.rgb *= 0.9;
   // if (avg_rgb(output.Color.rgb) > 0.9f ) 
   //     discard;
    
    
    return output;
    
}
