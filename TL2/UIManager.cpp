#include "UIManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

#include "ImGuiConsole.h"
#include "Vector.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "MemoryManager.h"
#include "StaticMeshActor.h"
#include"GizmoActor.h"
#include "World.h"

void UUIManager::Initialize(HWND hWindow, ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark(); // <-- 다크 테마 적용
    ImGui_ImplWin32_Init((void*)hWindow);
    ImGui_ImplDX11_Init(InDevice, InDeviceContext);
}

void UUIManager::Update(FVector& OutCameraLocation)
{
}

void UUIManager::BeginImGuiFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

extern float CameraYawDeg; // 월드 Up(Y) 기준 Yaw (무제한 누적)
extern float CameraPitchDeg; // 로컬 Right 기준 Pitch (제한됨)
extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

void UUIManager::RenderImGui()
{
    // === Jungle Control Panel ===
    ImGui::Begin("Jungle Control Panel", nullptr,
                 ImGuiWindowFlags_NoResize);

    ImGui::Text("Hello Jungle World!");
    ImGui::Text("FPS %.1f (%.3f ms)", ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    // Primitive 선택 콤보박스
    static int CurrentPrimitive = 0;
    const char* PrimitiveItems[] = {"Sphere", "Cube", "Triangle"};
    ImGui::Combo("Primitive", &CurrentPrimitive, PrimitiveItems, IM_ARRAYSIZE(PrimitiveItems));


    if (ImGui::Button("Spawn"))
    {
        //Spawn 처리
        if (WorldRef)
        {
            const char* Mesh =
                (CurrentPrimitive == 0) ? "Sphere.obj" : (CurrentPrimitive == 1) ? "Cube.obj" : "Triangle.obj";

            FTransform SpawnActorTF(
                FVector({0, 0, 0}),
                FQuat::MakeFromEuler({0, 0, 0}),
                FVector(1, 1, 1)
            );

            AStaticMeshActor* SpawnedActor = WorldRef->SpawnActor<AStaticMeshActor>(SpawnActorTF);
            //여기에 타입셋하는 코드 추가
            SpawnedActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);


            if (CurrentPrimitive == 0)
            {
                UE_LOG("Spawn Sphere");
            }
            if (CurrentPrimitive == 1)
            {
                UE_LOG("Spawn Cube");
            }
            if (CurrentPrimitive == 2)
            {
                UE_LOG("Spawn Triangle");
            }
        }
    }


    // Spawn 버튼 + 입력

    // 메모리 관리 stat창 임시구현
    // 메모리 메니저로 처리하긴 했는데 맥락상 도형만 말하는 것일듯?
    ImGui::Text("Number of spawn : %u ", CMemoryManager::TotalAllocationCount);
    ImGui::Text("Used Memory : %u Bytes", CMemoryManager::TotalAllocationBytes);

    // Scene 이름 입력 (UIManager 내부 버퍼 사용)
    ImGui::InputText("Scene Name", SceneNameBuf, (size_t)IM_ARRAYSIZE(SceneNameBuf));

    // Scene 버튼들
    if (ImGui::Button("New scene"))
    {
        std::snprintf(SceneNameBuf, sizeof(SceneNameBuf), "%s", "Default");
        WorldRef->CreateNewScene();
    }

    if (ImGui::Button("Save scene"))
    {
        WorldRef->SaveScene(SceneNameBuf);
    }

    if (ImGui::Button("Load scene"))
    {
        WorldRef->CreateNewScene();
        WorldRef->LoadScene(GetSceneName());
    }

    ImGui::Separator();

    // Camera 설정
    static bool Orthogonal = false;
    ImGui::Checkbox("Orthogonal", &Orthogonal);

    float CameraFOV = CameraActor->GetCameraComponent()->GetFOV();
    if (ImGui::SliderFloat("FOV", &CameraFOV, 1.0f, 179.0f))
    {
        CameraActor->GetCameraComponent()->SetFOV(CameraFOV);
    }

    // === Camera Location ===
    FVector CameraLocation = CameraActor->GetActorTransform().Translation;

    float CamLoc[3] = {CameraLocation.X, CameraLocation.Y, CameraLocation.Z};
    if (ImGui::SliderFloat3("Camera Location", CamLoc, -10.0f, 10.0f, "%.3f"))
    {
        CameraActor->SetActorLocation({CamLoc[0], CamLoc[1], CamLoc[2]});

        //CameraLocation.X = CamLoc[0];
        //CameraLocation.Y = CamLoc[1]; 
        //CameraLocation.Z = CamLoc[2];
    }


    // === Camera Rotation ===
    // 1. UI 표시를 위해 저장된 값을 배열에 복사
    UIRotationEuler[0] = StoredPitch;
    UIRotationEuler[1] = StoredYaw;
    UIRotationEuler[2] = StoredRoll;

    // 2. UI 슬라이더로 각 축을 개별 조작
    float PrevValues[3] = {UIRotationEuler[0], UIRotationEuler[1], UIRotationEuler[2]};
    //UE_LOG("UI CAMERA ROTATION      Pitch : %f  Yaw : %f Roll : %f", UIRotationEuler[0], UIRotationEuler[1], UIRotationEuler[2]);
    //UE_LOG("Real CAMERA ROTATION    Pitch : %f  Yaw : %f", CameraYawDeg, CameraPitchDeg);
    if (ImGui::SliderFloat3("Camera Rotation", UIRotationEuler, -180, 180, "%.1f"))
    {
        // 변경된 축 감지 및 단일축 회전 적용
        if (PrevValues[0] != UIRotationEuler[0]) // Pitch 변경
        {
            StoredPitch = NormalizeAngleDeg(UIRotationEuler[0]);
            ApplyAxisRotation(0, StoredPitch); // X축 회전

            //TempCameraDeg.X = StoredPitch;
        }
        else if (PrevValues[1] != UIRotationEuler[1]) // Yaw 변경
        {
            StoredYaw = NormalizeAngleDeg(UIRotationEuler[1]);
            ApplyAxisRotation(1, StoredYaw); // Y축 회전

            //TempCameraDeg.Y = StoredYaw;
        }
        else if (PrevValues[2] != UIRotationEuler[2]) // Roll 변경
        {
            StoredRoll = NormalizeAngleDeg(UIRotationEuler[2]);
            ApplyAxisRotation(2, StoredRoll); // Z축 회전

            //TempCameraDeg.Z = StoredRoll;
        }
        // 여기서 world의 CameraYawDeg에게 값전달해야함
        CameraPitchDeg = StoredPitch;
        CameraYawDeg = StoredYaw;
    }
    TempCameraDeg.X = StoredPitch;
    TempCameraDeg.Y = StoredYaw;
    TempCameraDeg.Z = StoredRoll;

    ImGui::End();

    // === Jungle Property Window ===
    ImGui::Begin("Jungle Property Window");

    if (PickedActor != nullptr)
    {
        FVector PALocation = PickedActor->GetActorLocation();
        FVector PARotation = PickedActor->GetActorRotation().ToEuler();
        FVector PAScale = PickedActor->GetActorScale();

        float PALocationVec[3] = {PALocation.X, PALocation.Y, PALocation.Z};
        float PARotationVec[3] = {PARotation.X, PARotation.Y, PARotation.Z};
        float PAScaleVec[3] = {PAScale.X, PAScale.Y, PAScale.Z};

        if (ImGui::InputFloat3("Translation", PALocationVec, 0, 0))
        {
            PickedActor->SetActorLocation({PALocationVec[0], PALocationVec[1], PALocationVec[2]});
        }
        if (ImGui::InputFloat3("Rotation", PARotationVec, 0, 0))
        {
            FVector ActorRotate = {PARotationVec[0], PARotationVec[1], PARotationVec[2]};
            PickedActor->SetActorRotation(ActorRotate);
        }
        if (ImGui::InputFloat3("Scale", PAScaleVec, 0, 0))
        {
            PickedActor->SetActorScale({PAScaleVec[0], PAScaleVec[1], PAScaleVec[2]});
        }
        static int GizmoModeIndex = static_cast<int>(GizmoActor->GetMode());
        const char* GizmoModes[] = {"Translate", "Rotate", "Scale"};

        ImGui::Text("Gizmo Mode:");
        // 실제 기즈모 모드를 가져와서 라디오 버튼 표시 상태 결정
        int CurrentMode = static_cast<int>(GizmoActor->GetMode());
        if (ImGui::RadioButton("Translate", CurrentMode == 0)) GizmoActor->SetMode(EGizmoMode::Translate);
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", CurrentMode == 1)) GizmoActor->SetMode(EGizmoMode::Rotate);
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", CurrentMode == 2)) GizmoActor->SetMode(EGizmoMode::Scale);


        static int SpaceIndex = 0;
        const char* GizmoSpaces[] = {"World", "Local"};
        ImGui::Combo("Gizmo Space", &SpaceIndex, GizmoSpaces, IM_ARRAYSIZE(GizmoSpaces));
        GizmoActor->SetSpaceWorldMatrix(static_cast<EGizmoSpace>(SpaceIndex), PickedActor);
    }

    ImGui::End();


    // === Console Panel ===
    static bool ShowConsole = true;
    ImGuiConsole::ShowExampleAppConsole(&ShowConsole);
}

void UUIManager::EndImGuiFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UUIManager::Release()
{
    if (bReleased) return;
    bReleased = true;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UUIManager::SetSceneName(const char* InName)
{
    if (!InName)
    {
        SceneNameBuf[0] = '\0';
        return;
    }
    std::snprintf(SceneNameBuf, sizeof(SceneNameBuf), "%s", InName);
}


void UUIManager::ResetPickedActor()
{
    PickedActor = nullptr;
}

void UUIManager::SetPickedActor(AActor* InPickedActor)
{
    PickedActor = InPickedActor;
}

AActor* UUIManager::GetPickedActor() const
{
    return PickedActor;
}

void UUIManager::SetGizmoActor(AGizmoActor* InGizmoActor)
{
    GizmoActor = InGizmoActor;
}

void UUIManager::SetCamera(ACameraActor* InCameraActor)
{
    CameraActor = InCameraActor;
}

void UUIManager::ApplyAxisRotation(int Axis, float AngleDegree)
{
    if (!CameraActor) return;

    // 축별 개별 쿼터니언 생성 및 합성
    FQuat PitchQuat = FQuat::FromAxisAngle(FVector(1, 0, 0), DegreeToRadian(StoredPitch));
    FQuat YawQuat = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreeToRadian(StoredYaw));
    FQuat RollQuat = FQuat::FromAxisAngle(FVector(0, 0, 1), DegreeToRadian(StoredRoll));

    // RzRxRy 순서로 회전 합성 (Roll(Z) → Pitch(X) → Yaw(Y))
    FQuat FinalRotation = YawQuat * PitchQuat * RollQuat;
    FinalRotation.Normalize();

    CameraActor->SetActorRotation(FinalRotation);
}

void UUIManager::UpdateMouseRotation(float Pitch, float Yaw)
{
    // 마우스로 변경된 Pitch/Yaw 값을 정규화하여 저장 (Roll은 유지)
    StoredPitch = NormalizeAngleDeg(Pitch);
    StoredYaw = NormalizeAngleDeg(Yaw);
    // StoredRoll은 변경하지 않음
}

void UUIManager::SetWorld(UWorld* InWorld)
{
    WorldRef = InWorld;
}

UWorld*& UUIManager::GetWorld()
{
    return WorldRef;
}

void UUIManager::OnWindowResize(int Width, int Height)
{
    LastClientSize = ImVec2((float)Width, (float)Height);

    // 디자인 해상도 대비 균등 스케일
    const float sx = Width / DesignRes.x;
    const float sy = Height / DesignRes.y;
    const float uniform = (sx < sy) ? sx : sy;

    ApplyImGuiScale(uniform);
}

void UUIManager::SetDesignResolution(int Width, int Height)
{
    DesignRes = ImVec2((float)Width, (float)Height);
    // 현재 창 크기 기준으로 즉시 재적용
    OnWindowResize((int)LastClientSize.x, (int)LastClientSize.y);
}

FVector UUIManager::GetTempCameraRotation() const
{
    return TempCameraDeg;
}

void UUIManager::ApplyImGuiScale(float NewScale)
{
    if (!bStyleCaptured)
    {
        BaseStyle = ImGui::GetStyle();
        bStyleCaptured = true;
    }

    if (fabsf(NewScale - UIScale) < 1e-3f) return;

    UIScale = NewScale;

    // 스타일은 "기준 스타일"에서 다시 계산(누적 스케일 방지)
    ImGuiStyle& style = ImGui::GetStyle();
    style = BaseStyle;
    style.ScaleAllSizes(UIScale);

    // 폰트는 빠르게 글로벌 스케일(큰 변화 땐 폰트 재빌드 권장)
    ImGui::GetIO().FontGlobalScale = UIScale;
}
