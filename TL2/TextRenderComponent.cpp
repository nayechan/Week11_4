#include "pch.h"
#include "Enums.h"
#include "d3dtk/DDSTextureLoader.h"
#include "TextRenderComponent.h"
#include "ResourceManager.h"
#include "VertexData.h"
#include "CameraActor.h"
UTextRenderComponent::UTextRenderComponent()
{
    TArray<uint32> Indices;
    for (uint32 i = 0;i < 100;i++)
    {
        Indices.push_back(i * 4 + 0);
        Indices.push_back(i * 4 + 1);
        Indices.push_back(i * 4 + 2);

        Indices.push_back(i * 4 + 2);
        Indices.push_back(i * 4 + 1);
        Indices.push_back(i * 4 + 3);
    }
	

    //if(UResourceManager::GetInstance().Get<UMaterial>())
    auto& RM = UResourceManager::GetInstance();
    TextQuad = RM.Get<UTextQuad>("TextBillboard");
    if (auto* M = RM.Get<UMaterial>("TextBillboard"))
    {
        Material = M;
    }
    else
    {
        Material = NewObject<UMaterial>();
        RM.Add<UMaterial>("TextBillboard", Material);
    }

    InitCharInfoMap();
}

UTextRenderComponent::~UTextRenderComponent()
{
    CharInfoMap.clear();
}



void UTextRenderComponent::InitCharInfoMap()
{
    const float TEXTURE_WH = 512.f;
    const float SUBTEX_WH = 32.f;
    const int COLROW = 16;

    //FTextureData* Data = new FTextureData();
    for (char c = 32; c <= 126;++c)
    {
        int key = c - 32;
        int col = key % COLROW;
        int row = key / COLROW;

        float pixelX = col * SUBTEX_WH;
        float pixelY = row * SUBTEX_WH;

        FBillboardVertexInfo CharInfo;
        CharInfo.UVRect.X = pixelX / TEXTURE_WH;
        CharInfo.UVRect.Y = pixelY / TEXTURE_WH;
        CharInfo.UVRect.Z = SUBTEX_WH / TEXTURE_WH;
        CharInfo.UVRect.W = CharInfo.UVRect.Z;

        CharInfoMap[c] = CharInfo;
    }
}

TArray<FBillboardVertexInfo_GPU> UTextRenderComponent::CreateVerticesForString(const FString& text, const FVector& StartPos) {
    TArray<FBillboardVertexInfo_GPU> vertices;
    auto* CharUVInfo = CharInfoMap.Find('A');
    float charWidth = (CharUVInfo->UVRect.Z) / (CharUVInfo->UVRect.W); //
    float CharHeight = 1.f;
    float CursorX = -charWidth*(text.size()/2);
    int vertexBaseIndex = 0;
    for (char c : text)
    {
        auto* CharUVInfo = CharInfoMap.Find(c);
        if (!CharUVInfo) continue;

        float u = CharUVInfo->UVRect.X;
        float v = CharUVInfo->UVRect.Y;
        float w = CharUVInfo->UVRect.Z; //32 / 512
        float h = CharUVInfo->UVRect.W; //32 / 512

        //float charWidth = (w / h);

        FBillboardVertexInfo_GPU Info;
        //1
        Info.Position[0] = CursorX;
        Info.Position[1] = CharHeight;
        Info.Position[2] = 0.f;

        Info.CharSize[0] = 1.f;
        Info.CharSize[1] = 1.f;

        Info.UVRect[0] = u;
        Info.UVRect[1] = v;
        vertices.push_back(Info);
        //2
        Info.Position[0] = CursorX+charWidth;
        Info.Position[1] = CharHeight;
        Info.Position[2] = 0.f;

        Info.CharSize[0] = 1.f;
        Info.CharSize[1] = 1.f;

        Info.UVRect[0] = u+w;
        Info.UVRect[1] = v;
        vertices.push_back(Info);
        //3
        Info.Position[0] = CursorX;
        Info.Position[1] = 0.f;
        Info.Position[2] = 0.f;

        Info.CharSize[0] = 1.f;
        Info.CharSize[1] = 1.f;

        Info.UVRect[0] = u;
        Info.UVRect[1] = v+h;
        vertices.push_back(Info);
        //4
        Info.Position[0] = CursorX+charWidth;
        Info.Position[1] = 0.f;
        Info.Position[2] = 0.f;

        Info.CharSize[0] = 1.f;
        Info.CharSize[1] = 1.f;

        Info.UVRect[0] = u+w;
        Info.UVRect[1] = v+h;
        vertices.push_back(Info);

        CursorX += charWidth;
        vertexBaseIndex += 4;
    }
    return vertices;
}

void UTextRenderComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (Owner->GetIsPicked() == true)
    {
        Material->Load("TextBillboard.dds", Renderer->GetRHIDevice()->GetDevice());//texture 불러오기 초기화는 ResourceManager Initialize() -> CreateTextBillboardTexture();
        ACameraActor* CameraActor = GetOwner()->GetWorld()->GetCameraActor();
        FVector CamRight = CameraActor->GetActorRight();
        FVector CamUp = CameraActor->GetActorUp();


        FVector cameraPosition = CameraActor->GetActorLocation();
        Renderer->UpdateBillboardConstantBuffers(Owner->GetActorLocation() + FVector(0.f, 0.f, 1.f) * Owner->GetActorScale().Z, View, Proj, CamRight, CamUp);


        Renderer->PrepareShader(Material->GetShader());
        TArray<FBillboardVertexInfo_GPU> vertices = CreateVerticesForString(FString("UUID : ") + FString(std::to_string(Owner->UUID)), Owner->GetActorLocation());//TODO : HELLOWORLD를 멤버변수 TEXT로바꾸기
        UResourceManager::GetInstance().UpdateDynamicVertexBuffer("TextBillboard", vertices);
        Renderer->OMSetBlendState(true);
        Renderer->RSSetState(EViewModeIndex::VMI_Unlit);
        Renderer->DrawIndexedPrimitiveComponent(this, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Renderer->OMSetBlendState(false);
    }
}
