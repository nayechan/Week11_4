#include "pch.h"
#include "FViewport.h"
#include "FViewportClient.h"

FViewport::FViewport()
{
}

FViewport::~FViewport()
{
    Cleanup();
}

bool FViewport::Initialize(float InStartX, float InStartY, float InSizeX, float InSizeY, ID3D11Device* Device)
{
    if (!Device)
        return false;

    D3DDevice = Device;
    D3DDevice->GetImmediateContext(&D3DDeviceContext);
    StartX = static_cast<uint32>(InStartX);
    StartY = static_cast<uint32>(InStartY);
    SizeX = static_cast<uint32>(InSizeX);
    SizeY = static_cast<uint32>(InSizeY);

    CreateRenderTargets();

    return IsValid();
}

void FViewport::Cleanup()
{
    ReleaseRenderTargets();

    if (D3DDeviceContext)
    {
        D3DDeviceContext->Release();
        D3DDeviceContext = nullptr;
    }

    D3DDevice = nullptr;
}

void FViewport::BeginRenderFrame()
{
    // 뷰포트 설정
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(SizeX);
    viewport.Height = static_cast<float>(SizeY);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = static_cast<float>(StartX);
    viewport.TopLeftY = static_cast<float>(StartY);
    D3DDeviceContext->RSSetViewports(1, &viewport);
}

void FViewport::EndRenderFrame()
{
    // 렌더링 완료 후 처리할 작업이 있다면 여기에 추가
}

void FViewport::Present()
{
    // 스왑체인이 있다면 Present 호출
    // 현재는 오프스크린 렌더링이므로 생략
}

void FViewport::Resize(uint32 NewStartX, uint32 NewStartY,uint32 NewSizeX, uint32 NewSizeY)
{
    if (SizeX == NewSizeX && SizeY == NewSizeY)
        return;
    StartX = NewStartX;
    StartY = NewStartY;
    SizeX = NewSizeX;
    SizeY = NewSizeY;

    ReleaseRenderTargets();
    CreateRenderTargets();
}

void FViewport::SetMainViewport()
{
    MainViewport = true;
}

void FViewport::ProcessMouseMove(int32 X, int32 Y)
{

    if (ViewportClient)
    {
        ViewportMousePosition.X = static_cast<float>(X);
        ViewportMousePosition.Y = static_cast<float>(Y);
        ViewportClient->MouseMove(this, X, Y);
    }
}

void FViewport::ProcessMouseButtonDown(int32 X, int32 Y, int32 Button)
{
    if (ViewportClient)
    {
        ViewportClient->MouseButtonDown(this, X, Y, Button);
    }
}

void FViewport::ProcessMouseButtonUp(int32 X, int32 Y, int32 Button)
{
    if (ViewportClient)
    {
        ViewportClient->MouseButtonUp(this, X, Y, Button);
    }
}

void FViewport::ProcessKeyDown(int32 KeyCode)
{
    if (ViewportClient)
    {
        ViewportClient->KeyDown(this, KeyCode);
    }
}

void FViewport::ProcessKeyUp(int32 KeyCode)
{
    if (ViewportClient)
    {
        ViewportClient->KeyUp(this, KeyCode);
    }
}

void FViewport::CreateRenderTargets()
{
    if (!D3DDevice || SizeX == 0 || SizeY == 0)
        return;

    // 렌더 타겟 텍스처 생성
    D3D11_TEXTURE2D_DESC rtDesc = {};
    rtDesc.Width = SizeX;
    rtDesc.Height = SizeY;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.SampleDesc.Quality = 0;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    rtDesc.CPUAccessFlags = 0;
    rtDesc.MiscFlags = 0;

    HRESULT hr = D3DDevice->CreateTexture2D(&rtDesc, nullptr, &RenderTargetTexture);
    if (FAILED(hr))
        return;

    // 렌더 타겟 뷰 생성
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = rtDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    hr = D3DDevice->CreateRenderTargetView(RenderTargetTexture, &rtvDesc, &RenderTargetView);
    if (FAILED(hr))
        return;

    // 셰이더 리소스 뷰 생성 (ImGui에서 사용)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = rtDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = D3DDevice->CreateShaderResourceView(RenderTargetTexture, &srvDesc, &ShaderResourceView);
    if (FAILED(hr))
        return;

    // 깊이 스텐실 텍스처 생성
    D3D11_TEXTURE2D_DESC dsDesc = {};
    dsDesc.Width = SizeX;
    dsDesc.Height = SizeY;
    dsDesc.MipLevels = 1;
    dsDesc.ArraySize = 1;
    dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.SampleDesc.Count = 1;
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Usage = D3D11_USAGE_DEFAULT;
    dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dsDesc.CPUAccessFlags = 0;
    dsDesc.MiscFlags = 0;

    hr = D3DDevice->CreateTexture2D(&dsDesc, nullptr, &DepthStencilTexture);
    if (FAILED(hr))
        return;

    // 깊이 스텐실 뷰 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = dsDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = D3DDevice->CreateDepthStencilView(DepthStencilTexture, &dsvDesc, &DepthStencilView);
}

void FViewport::ReleaseRenderTargets()
{
    if (ShaderResourceView)
    {
        ShaderResourceView->Release();
        ShaderResourceView = nullptr;
    }

    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }

    if (DepthStencilTexture)
    {
        DepthStencilTexture->Release();
        DepthStencilTexture = nullptr;
    }

    if (RenderTargetView)
    {
        RenderTargetView->Release();
        RenderTargetView = nullptr;
    }

    if (RenderTargetTexture)
    {
        RenderTargetTexture->Release();
        RenderTargetTexture = nullptr;
    }
}