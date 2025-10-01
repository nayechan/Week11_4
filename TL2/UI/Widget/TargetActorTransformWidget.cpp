#include "pch.h"
#include "TargetActorTransformWidget.h"
#include "UI/UIManager.h"
#include "ImGui/imgui.h"
#include "Actor.h"
#include "GridActor.h"
#include "World.h"
#include "Vector.h"
#include "GizmoActor.h"
#include <string>
#include "StaticMeshActor.h"    
#include "StaticMeshComponent.h"
#include "ResourceManager.h"    
#include "TextRenderComponent.h"
#include "CameraComponent.h"
#include "BillboardComponent.h"
using namespace std;

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

// ★ 고정 오더: ZYX (Yaw-Pitch-Roll) — 기즈모의 Delta 곱(Z * Y * X)과 동일
static inline FQuat QuatFromEulerZYX_Deg(const FVector& Deg)
{
	const float Rx = DegreeToRadian(Deg.X); // Roll (X)
	const float Ry = DegreeToRadian(Deg.Y); // Pitch (Y)
	const float Rz = DegreeToRadian(Deg.Z); // Yaw (Z)

	const FQuat Qx = MakeQuatFromAxisAngle(FVector(1, 0, 0), Rx);
	const FQuat Qy = MakeQuatFromAxisAngle(FVector(0, 1, 0), Ry);
	const FQuat Qz = MakeQuatFromAxisAngle(FVector(0, 0, 1), Rz);
	return Qz * Qy * Qx; // ZYX
}

static inline FVector EulerZYX_DegFromQuat(const FQuat& Q)
{
	// 표준 ZYX(roll=x, pitch=y, yaw=z) 복원식
	// 참고: roll(X), pitch(Y), yaw(Z)
	const float w = Q.W, x = Q.X, y = Q.Y, z = Q.Z;

	const float sinr_cosp = 2.0f * (w * x + y * z);
	const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
	float roll = std::atan2(sinr_cosp, cosr_cosp);

	float sinp = 2.0f * (w * y - z * x);
	float pitch;
	if (std::fabs(sinp) >= 1.0f) pitch = std::copysign(HALF_PI, sinp);
	else                          pitch = std::asin(sinp);

	const float siny_cosp = 2.0f * (w * z + x * y);
	const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
	float yaw = std::atan2(siny_cosp, cosy_cosp);

	return FVector(RadianToDegree(roll), RadianToDegree(pitch), RadianToDegree(yaw));
}

namespace
{
	struct FAddableComponentDescriptor
	{
		const char* Label;
		UClass* Class;
		const char* Description;
	};

	const TArray<FAddableComponentDescriptor>& GetAddableComponentDescriptors()
	{
		static TArray<FAddableComponentDescriptor> Options = []()
			{
				TArray<FAddableComponentDescriptor> Result;
				Result.push_back({ "Static Mesh Component", UStaticMeshComponent::StaticClass(), "Static mesh 렌더링용 컴포넌트" });
				Result.push_back({ "Camera Component", UCameraComponent::StaticClass(), "카메라 뷰/프로젝션 제공" });
				Result.push_back({ "Text Render Component", UTextRenderComponent::StaticClass(), "텍스트 표시" });
				Result.push_back({ "Line Component", ULineComponent::StaticClass(), "라인/디버그 드로잉" });
				Result.push_back({ "AABB Component", UAABoundingBoxComponent::StaticClass(), "바운딩 박스 시각화" });
				Result.push_back({ "Billboard Component", UBillboardComponent::StaticClass(), "빌보드 텍스쳐 표시" });
				return Result;
			}();
		return Options;
	}
	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass)
	{
		if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			return false;

		UObject* RawObject = ObjectFactory::NewObject(ComponentClass);
		if (!RawObject)
		{
			return false;
		}

		UActorComponent* NewComp = Cast<UActorComponent>(RawObject);
		if (!NewComp)
		{
			ObjectFactory::DeleteObject(RawObject);
			return false;
		}

		NewComp->SetOwner(&Actor);

		// 씬 컴포넌트라면 루트에 붙임
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
		{
			SceneComp->SetWorldTransform(Actor.GetActorTransform()); // 초기 트랜스폼
			if (USceneComponent* Root = Actor.GetRootComponent())
			{
				SceneComp->SetupAttachment(Root, EAttachmentRule::KeepRelative);
			}
		}

		// AddOwnedComponent 경유 (Register/Initialize 포함)
		Actor.AddOwnedComponent(NewComp);
		Actor.MarkPartitionDirty();
		return true;
	}
	void MarkComponentSubtreeVisited(USceneComponent* Component, TSet<USceneComponent*>& Visited)
	{
		if (!Component || Visited.count(Component))
		{
			return;
		}

		Visited.insert(Component);
		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			MarkComponentSubtreeVisited(Child, Visited);
		}
	}

	void RenderSceneComponentTree(
		USceneComponent* Component,
		AActor& Actor,
		USceneComponent*& SelectedComponent,
		USceneComponent*& ComponentPendingRemoval,
		TSet<USceneComponent*>& Visited)
	{
		if (!Component)
			return;

		Visited.insert(Component);

		const TArray<USceneComponent*>& Children = Component->GetAttachChildren();
		const bool bHasChildren = !Children.IsEmpty();

		ImGuiTreeNodeFlags NodeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (!bHasChildren)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
		if (Component == SelectedComponent)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		FString Label = Component->GetClass() ? Component->GetClass()->Name : "Unknown Component";
		if (Component == Actor.GetRootComponent())
		{
			Label += " (Root)";
		}

		// 트리 노드 그리기 직전에 ID 푸시
		ImGui::PushID(Component);
		const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
		// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			SelectedComponent = Component;
		}

		if (ImGui::BeginPopupContextItem("ComponentContext"))
		{
			const bool bCanRemove = (Component != Actor.GetRootComponent());
			if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
			{
				ComponentPendingRemoval = Component;
			}
			ImGui::EndPopup();
		}

		if (bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				RenderSceneComponentTree(Child, Actor, SelectedComponent, ComponentPendingRemoval, Visited);
			}
			ImGui::TreePop();
		}
		else if (!bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				MarkComponentSubtreeVisited(Child, Visited);
			}
		}
		// 항목 처리가 끝나면 반드시 PopID
		ImGui::PopID();
	}
}

// 파일명 스템(Cube 등) 추출 + .obj 확장자 제거
static inline FString GetBaseNameNoExt(const FString& Path)
{
	const size_t sep = Path.find_last_of("/\\");
	const size_t start = (sep == FString::npos) ? 0 : sep + 1;

	const FString ext = ".obj";
	size_t end = Path.size();
	if (end >= ext.size() && Path.compare(end - ext.size(), ext.size(), ext) == 0)
	{
		end -= ext.size();
	}
	if (start <= end) return Path.substr(start, end - start);
	return Path;
}

UTargetActorTransformWidget::UTargetActorTransformWidget()
	: UWidget("Target Actor Transform Widget")
	, UIManager(&UUIManager::GetInstance())
{

}

UTargetActorTransformWidget::~UTargetActorTransformWidget() = default;

void UTargetActorTransformWidget::OnSelectedActorCleared()
{
	// 즉시 내부 캐시/플래그 정리
	SelectedActor = nullptr;
	CachedActorName.clear();
	ResetChangeFlags();
	SelectedComponent = nullptr;
}

void UTargetActorTransformWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();

	// Transform 위젯을 UIManager에 등록하여 선택 해제 브로드캐스트를 받을 수 있게 함
	if (UIManager)
	{
		UIManager->RegisterTargetTransformWidget(this);
	}
}

AActor* UTargetActorTransformWidget::GetCurrentSelectedActor() const
{
	if (!UIManager)
		return nullptr;
		
	return UIManager->GetSelectedActor();
}

void UTargetActorTransformWidget::Update()
{
	// UIManager를 통해 현재 선택된 액터 가져오기
	AActor* CurrentSelectedActor = GetCurrentSelectedActor();
	if (SelectedActor != CurrentSelectedActor)
	{
		SelectedActor = CurrentSelectedActor;
		SelectedComponent = nullptr;
		// 새로 선택된 액터의 이름 캐시
		if (SelectedActor)
		{
			try
			{
				CachedActorName = SelectedActor->GetName().ToString();

				// ★ 선택 변경 시 한 번만 초기화
				const FVector S = SelectedActor->GetActorScale();
				bUniformScale = (fabs(S.X - S.Y) < 0.01f && fabs(S.Y - S.Z) < 0.01f);

				// 스냅샷
				UpdateTransformFromActor();
				PrevEditRotationUI = EditRotation; // ★ 회전 UI 기준값 초기화
				bRotationEditing = false;          // ★ 편집 상태 초기화
			}
			catch (...)
			{
				CachedActorName = "[Invalid Actor]";
				SelectedActor = nullptr;
			}
		}
		else
		{
			CachedActorName.clear();
		}
	}

	if (SelectedActor)
	{
		// 액터가 선택되어 있으면 항상 트랜스폼 정보를 업데이트하여
		// 기즈모 조작을 실시간으로 UI에 반영합니다.
		// 회전 필드 편집 중이면 그 프레임은 엔진→UI 역동기화(회전)를 막는다.
		if (!bRotationEditing)
		{
			UpdateTransformFromActor();
			// 회전을 제외하고 위치/스케일도 여기서 갱신하고 싶다면,
			// UpdateTransformFromActor()에서 회전만 건너뛰는 오버로드를 따로 만들어도 OK.
		}
	}
}

void UTargetActorTransformWidget::RenderWidget()
{
	{
		/*
			컨트롤 패널에서 선택한 직후, Update()가 아직 돌기 전에
			RenderWidget()이 먼저 실행되면 CachedActorName이 빈 상태인 채로 Selectable을
			그리게 되기 때문에 여기서도 캐시 갱신
		*/

		// 캐시 갱신을 RenderWidget에서도 보장
		if (SelectedActor)
		{
			try
			{
				const FString LatestName = SelectedActor->GetName().ToString();
				if (CachedActorName != LatestName)
				{
					CachedActorName = LatestName;
				}
			}
			catch (...)
			{
				CachedActorName.clear();
				SelectedActor = nullptr;
			}
		}
		else
		{
			CachedActorName.clear();
		}
		
	}
	// 컴포넌트 관리 UI
	if (SelectedActor)
	{
		ImGui::Text(CachedActorName.c_str());
		ImGui::SameLine();
		// 버튼 크기
		const float ButtonWidth = 60.0f;
		const float ButtonHeight = 25.0f;
		// 남은 영역에서 버튼 폭만큼 오른쪽으로 밀기 (클램프)
		float Avail = ImGui::GetContentRegionAvail().x;
		float NewX = ImGui::GetCursorPosX() + std::max(0.0f, Avail - ButtonWidth);
		ImGui::SetCursorPosX(NewX);
		if (ImGui::Button("+ 추가", ImVec2(ButtonWidth, ButtonHeight)))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::TextUnformatted("Add Component");
			ImGui::Separator();

			for (const FAddableComponentDescriptor& Descriptor : GetAddableComponentDescriptors())
			{
				ImGui::PushID(Descriptor.Label);
				if (ImGui::Selectable(Descriptor.Label))
				{
					if (TryAttachComponentToActor(*SelectedActor, Descriptor.Class))
					{
						ImGui::CloseCurrentPopup();
					}
				}
				if (Descriptor.Description && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Descriptor.Description);
				}
				ImGui::PopID();
			}

			ImGui::EndPopup();
		}
		ImGui::Spacing();


		AActor* ActorPendingRemoval = nullptr;
		USceneComponent* ComponentPendingRemoval = nullptr;
		USceneComponent* RootComponent = SelectedActor->GetRootComponent();
		const bool bActorSelected = (SelectedActor != nullptr && SelectedComponent == nullptr);

		// 1) 컴포넌트 트리 박스 크기 관련
		static float PaneHeight = 120.0f;        // 초기값
		const float SplitterThickness = 6.0f;    // 드래그 핸들 두께
		const float MinTop = 1.0f;             // 위 박스 최소 높이
		const float MinBottom = 0.0f;          // 아래 영역 최소 높이

		// 현재 창의 남은 영역 높이(이 함수 블록의 시작 시점 기준)
		const float WindowAvailY = ImGui::GetContentRegionAvail().y;

		// 창이 줄어들었을 때 위/아래 최소 높이를 고려해 클램프
		PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, WindowAvailY - (MinBottom + SplitterThickness)));

		ImGui::BeginChild("ComponentBox", ImVec2(0, PaneHeight), true);
		ImGui::PushID("ActorDisplay");
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));

		// 액터 이름 표시 
		const bool bActorClicked =
			ImGui::Selectable(CachedActorName.c_str(), bActorSelected, ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_SpanAvailWidth);
		ImGui::PopStyleColor();

		if (bActorClicked && SelectedActor)
		{
			SelectedComponent = nullptr;
		}

		if (ImGui::BeginPopupContextItem("ActorContextMenu"))
		{
			if (ImGui::MenuItem("삭제", "Delete", false, SelectedActor != nullptr))
			{
				ActorPendingRemoval = SelectedActor;
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (!RootComponent)
		{
			ImGui::BulletText("Root component not found.");
		}
		else
		{
			TSet<USceneComponent*> VisitedComponents;
			// 아직 루트 컴포넌트에 붙은 컴포넌트는 static component밖에 없음.
			RenderSceneComponentTree(RootComponent, *SelectedActor, SelectedComponent, ComponentPendingRemoval, VisitedComponents);

			const TArray<USceneComponent*>& AllComponents = SelectedActor->GetSceneComponents();
			for (USceneComponent* Component : AllComponents)
			{
				if (!Component || VisitedComponents.count(Component))
				{
					//UE_LOG("Skipping already visited or null component.");
					continue;
				}

				ImGui::PushID(Component);
				const bool bSelected = (Component == SelectedComponent);
				if (ImGui::Selectable(Component->GetClass()->Name, bSelected))
				{
					SelectedComponent = Component; // 하이라이트 유지
				}

				if (ImGui::BeginPopupContextItem("ComponentContext"))
				{
					// 루트 컴포넌트가 아닌 경우에만 제거 가능
					const bool bCanRemove = (Component != RootComponent);
					if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
					{
						ComponentPendingRemoval = Component;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
		}
		const bool bDeletePressed =
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			ImGui::IsKeyPressed(ImGuiKey_Delete);

		if (bDeletePressed && SelectedActor)
		{
			if (SelectedComponent == nullptr)
			{
				// 액터 Row가 선택된 상태 → 액터 삭제
				ActorPendingRemoval = SelectedActor;
			}
			else if (SelectedComponent != RootComponent)
			{
				// 컴포넌트 선택 상태(루트 아님) → 해당 컴포넌트 삭제
				ComponentPendingRemoval = SelectedComponent;
			}
			else
			{
				// 루트 컴포넌트가 선택된 상태면 삭제 불가
			}
		}

		if (ComponentPendingRemoval)
		{
			SelectedActor->RemoveOwnedComponent(ComponentPendingRemoval);
			if (SelectedComponent == ComponentPendingRemoval)
			{
				SelectedComponent = nullptr;
			}
		}

		if (ActorPendingRemoval)
		{
			UWorld* World = ActorPendingRemoval->GetWorld();
			if (World)
			{
				World->DestroyActor(ActorPendingRemoval);
			}
			else
			{
				ActorPendingRemoval->Destroy();
			}
			OnSelectedActorCleared();
			return; // 방금 제거된 액터에 대한 나머지 UI 갱신 건너뜀
		}

		ImGui::EndChild();
		// 2) 스플리터(드래그 핸들)
		// 화면 가로로 꽉 차는 보이지 않는 버튼을 만든다.
		ImGui::PushStyleColor(ImGuiCol_Separator, ImGui::GetStyle().Colors[ImGuiCol_Separator]);
		ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, 0)); // 영향 거의 없음, 습관적 가드
		ImGui::InvisibleButton("VerticalSplitter", ImVec2(-FLT_MIN, SplitterThickness));
		bool SplitterHovered = ImGui::IsItemHovered();
		bool SplitterActive = ImGui::IsItemActive();

		// 커서 모양 변경
		if (SplitterHovered || SplitterActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		// 드래그 중이면 높이 조정
		if (SplitterActive)
		{
			PaneHeight += ImGui::GetIO().MouseDelta.y;
			// 다시 클램프 (위/아래 최소 높이 유지)
			PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, ImGui::GetContentRegionAvail().y + PaneHeight /* 아래 계산상 보정 */
				+ SplitterThickness - MinBottom));
		}

		// 얇은 선을 그려 시각적 구분(선택)
		{
			ImVec2 Min = ImGui::GetItemRectMin();
			ImVec2 Max = ImGui::GetItemRectMax();
			float Y = 0.5f * (Min.y + Max.y);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y),
				ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::Spacing();
		
		// Location 편집
		if (ImGui::DragFloat3("Location", &EditLocation.X, 0.1f))
		{
			bPositionChanged = true;
		}
		
		// ───────── Rotation: DragFloat3 하나로 "드래그=증분", "입력=절대" 처리 ─────────
		{
			// 1) 컨트롤 그리기 전에 이전값 스냅
			const FVector Before = EditRotation;

			// 2) DragFloat3 호출
			//    (키보드 입력도 허용되는 컨트롤이므로 라벨에 ZYX 오더 명시 권장)
			bool ChangedThisFrame = ImGui::DragFloat3("Rotation", &EditRotation.X, 0.5f);
			// 3) 컨트롤 상태 읽기
			ImGuiIO& IO = ImGui::GetIO();
			const bool Activated = ImGui::IsItemActivated();              // 이번 프레임에 포커스/활성 시작
			const bool Active = ImGui::IsItemActive();                 // 현재 편집 중
			const bool Edited = ImGui::IsItemEdited();                 // 이번 프레임에 값이 변함
			const bool Deactivated = ImGui::IsItemDeactivatedAfterEdit();   // 편집 종료(값 변함 포함)

			// "드래그 중" 판정: 마우스 좌클릭이 눌린 상태에서 활성이고, 실제 드래그가 발생
			const bool MouseHeld = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			const bool MouseDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) ||
				(std::fabs(IO.MouseDelta.x) + std::fabs(IO.MouseDelta.y) > 0.0f);
			const bool Dragging = Active && MouseHeld && MouseDrag;

			// 4) 편집 세션 시작 시(이번 프레임 Activate) 기준값 스냅
			//    - 절대 적용용으로 쓸 시작 회전과 UI 기준값 저장
			static FQuat StartQuatOnEdit;
			if (Activated)
			{
				StartQuatOnEdit = SelectedActor ? SelectedActor->GetActorRotation() : FQuat();
				PrevEditRotationUI = EditRotation;
				bRotationEditing = true;   // Update() 역동기화 방지
			}

			// 5) 값이 변한 프레임에 처리
			if (Edited && SelectedActor)
			{
				if (Dragging)
				{
					// ── 드래그: "증분 누적" ──
					FVector DeltaEuler = EditRotation - PrevEditRotationUI;

					auto Wrap = [](float a)->float { while (a > 180.f) a -= 360.f; while (a < -180.f) a += 360.f; return a; };
					DeltaEuler.X = Wrap(DeltaEuler.X);
					DeltaEuler.Y = Wrap(DeltaEuler.Y);
					DeltaEuler.Z = Wrap(DeltaEuler.Z);

					const FQuat Qx = MakeQuatFromAxisAngle(FVector(1, 0, 0), DegreeToRadian(DeltaEuler.X));
					const FQuat Qy = MakeQuatFromAxisAngle(FVector(0, 1, 0), DegreeToRadian(DeltaEuler.Y));
					const FQuat Qz = MakeQuatFromAxisAngle(FVector(0, 0, 1), DegreeToRadian(DeltaEuler.Z));
					const FQuat DeltaQuat = Qz * Qy * Qx; // ZYX

					FQuat Cur = SelectedActor->GetActorRotation();
					SelectedActor->SetActorRotation(DeltaQuat * Cur);

					// 다음 증분 기준 업데이트
					PrevEditRotationUI = EditRotation;
					bRotationChanged = false; // PostProcess에서 중복 적용 방지
				}
				else
				{
					// ── 키보드 입력: "절대 적용" ──
					// (편집 중간에도 즉시 절대값을 적용해 반영)
					const FQuat NewQ = QuatFromEulerZYX_Deg(EditRotation);
					SelectedActor->SetActorRotation(NewQ);

					// 표시값을 결과에 스냅(짝함수라 값 유지됨)
					EditRotation = EulerZYX_DegFromQuat(NewQ);
					PrevEditRotationUI = EditRotation;
					bRotationChanged = false;
				}
			}

			// 6) 편집 종료 시(포커스 빠짐) 최종 스냅 & 상태 리셋
			if (Deactivated)
			{
				if (SelectedActor)
				{
					EditRotation = EulerZYX_DegFromQuat(SelectedActor->GetActorRotation());
					PrevEditRotationUI = EditRotation;
				}
				bRotationEditing = false;
			}
		}


		// Scale 편집
		ImGui::Checkbox("Uniform Scale", &bUniformScale);
		
		if (bUniformScale)
		{
			float UniformScale = EditScale.X;
			if (ImGui::DragFloat("Scale", &UniformScale, 0.01f, 0.01f, 10.0f))
			{
				EditScale = FVector(UniformScale, UniformScale, UniformScale);
				bScaleChanged = true;
			}
		}
		else
		{
			if (ImGui::DragFloat3("Scale", &EditScale.X, 0.01f, 0.01f, 10.0f))
			{
				bScaleChanged = true;
			}
		}
		
		ImGui::Spacing();
		
		// 실시간 적용 버튼
		// TODO (동민) : 아마 이 부분은 나중에 삭제될 것 같습니다. 기즈모 조작이 실시간으로 반영되기 때문에.
		//if (ImGui::Button("Apply Transform"))
		//{
		//	ApplyTransformToActor();
		//}
		//
		//ImGui::SameLine();
		//if (ImGui::Button("Reset Transform"))
		//{
		//	UpdateTransformFromActor();
		//	ResetChangeFlags();
		//}
		
		ImGui::Spacing();
		ImGui::Separator();

		// Actor가 AStaticMeshActor인 경우 StaticMesh 변경 UI
		{
			if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(SelectedActor))
			{
				UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent();

				ImGui::Text("Static Mesh Override");
				if (!SMC)
				{
					ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "StaticMeshComponent not found.");
				}
				else
				{
					// 현재 메시 경로 표시
					FString CurrentPath;
					UStaticMesh* CurMesh = SMC->GetStaticMesh();
					if (CurMesh)
					{
						CurrentPath = CurMesh->GetAssetPathFileName();
						ImGui::Text("Current: %s", CurrentPath.c_str());
					}
					else
					{
						ImGui::Text("Current: <None>");
					}

					// 리소스 매니저에서 로드된 모든 StaticMesh 경로 수집
					auto& RM = UResourceManager::GetInstance();
					TArray<FString> Paths = RM.GetAllStaticMeshFilePaths();

					if (Paths.empty())
					{
						ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "No StaticMesh resources loaded.");
					}
					else
					{
						// 표시용 이름(파일명 스템)
						TArray<FString> DisplayNames;
						DisplayNames.reserve(Paths.size());
						for (const FString& p : Paths)
							DisplayNames.push_back(GetBaseNameNoExt(p));

						// ImGui 콤보 아이템 배열
						TArray<const char*> Items;
						Items.reserve(DisplayNames.size());
						for (const FString& n : DisplayNames)
							Items.push_back(n.c_str());

						// 선택 인덱스 유지
						static int SelectedMeshIdx = -1;

						// 기본 선택: Cube가 있으면 자동 선택
						if (SelectedMeshIdx == -1)
						{
							for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
							{
								if (DisplayNames[i] == "Cube" || Paths[i] == "Data/Cube.obj")
								{
									SelectedMeshIdx = i;
									break;
								}
							}
						}

						ImGui::SetNextItemWidth(240);
						ImGui::Combo("StaticMesh", &SelectedMeshIdx, Items.data(), static_cast<int>(Items.size()));
						if (ImGui::Button("Apply Mesh"))
						{
							if (SelectedMeshIdx >= 0 && SelectedMeshIdx < static_cast<int>(Paths.size()))
							{
								const FString& NewPath = Paths[SelectedMeshIdx];
								SMC->SetStaticMesh(NewPath);

								// Sphere 충돌 특례
								if (GetBaseNameNoExt(NewPath) == "Sphere")
									SMActor->SetCollisionComponent(EPrimitiveType::Sphere);
								else
									SMActor->SetCollisionComponent();

								UE_LOG("Applied StaticMesh: %s", NewPath.c_str());
							}
						}

						// 현재 메시로 선택 동기화 버튼 (옵션)
						ImGui::SameLine();
						if (ImGui::Button("Select Current"))
						{
							SelectedMeshIdx = -1;
							if (!CurrentPath.empty())
							{
								for (int i = 0; i < static_cast<int>(Paths.size()); ++i)
								{
									if (Paths[i] == CurrentPath ||
										DisplayNames[i] == GetBaseNameNoExt(CurrentPath))
									{
										SelectedMeshIdx = i;
										break;
									}
								}
							}
						}
					}

					// Material 설정
					ImGui::Separator();

					const TArray<FString> MaterialNames = UResourceManager::GetInstance().GetAllFilePaths<UMaterial>();
					// ImGui 콤보 아이템 배열
					TArray<const char*> MaterialNamesCharP;
					MaterialNamesCharP.reserve(MaterialNames.size());
					for (const FString& n : MaterialNames)
						MaterialNamesCharP.push_back(n.c_str());

					if (CurMesh)
					{
						const uint64 MeshGroupCount = CurMesh->GetMeshGroupCount();

						static TArray<int32> SelectedMaterialIdxAt; // i번 째 Material Slot이 가지고 있는 MaterialName이 MaterialNames의 몇번쩨 값인지.
						if (SelectedMaterialIdxAt.size() < MeshGroupCount)
						{
							SelectedMaterialIdxAt.resize(MeshGroupCount);
						}

						// 현재 SMC의 MaterialSlots 정보를 UI에 반영
						const TArray<FMaterialSlot>& MaterialSlots = SMC->GetMaterailSlots();
						for (uint64 MaterialSlotIndex = 0; MaterialSlotIndex < MeshGroupCount; ++MaterialSlotIndex)
						{
							for (uint32 MaterialIndex = 0; MaterialIndex < MaterialNames.size(); ++MaterialIndex)
							{
								if (MaterialSlots[MaterialSlotIndex].MaterialName == MaterialNames[MaterialIndex])
								{
									SelectedMaterialIdxAt[MaterialSlotIndex] = MaterialIndex;
								}
							}
						}

						// Material 선택
						for (uint64 MaterialSlotIndex = 0; MaterialSlotIndex < MeshGroupCount; ++MaterialSlotIndex)
						{
							ImGui::PushID(static_cast<int>(MaterialSlotIndex));
							if (ImGui::Combo("Material", &SelectedMaterialIdxAt[MaterialSlotIndex], MaterialNamesCharP.data(), static_cast<int>(MaterialNamesCharP.size())))
							{
								SMC->SetMaterialByUser(static_cast<uint32>(MaterialSlotIndex), MaterialNames[SelectedMaterialIdxAt[MaterialSlotIndex]]);
							}
							ImGui::PopID();
						}
					}
				}
			}
			else
			{
				ImGui::Text("Selected actor is not a StaticMeshActor.");
			}
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Actor Selected");
		ImGui::TextUnformatted("Select an actor to edit its transform.");
	}
	
	ImGui::Separator();
}

void UTargetActorTransformWidget::PostProcess()
{
	// 자동 적용이 활성화된 경우 변경사항을 즉시 적용
	if (bPositionChanged || bRotationChanged || bScaleChanged)
	{
		ApplyTransformToActor();
		ResetChangeFlags(); // 적용 후 플래그 리셋
	}
}

void UTargetActorTransformWidget::UpdateTransformFromActor()
{
	if (!SelectedActor)
		return;
		
	// 액터의 현재 트랜스폼을 UI 변수로 복사
	EditLocation = SelectedActor->GetActorLocation();
	//EditRotation = SelectedActor->GetActorRotation().ToEuler();
	// ★ 표시는 ZYX 기준으로
	EditRotation = EulerZYX_DegFromQuat(SelectedActor->GetActorRotation());
	EditScale = SelectedActor->GetActorScale();
	
	ResetChangeFlags();
}

void UTargetActorTransformWidget::ApplyTransformToActor() const
{
	if (!SelectedActor)
		return;
		
	// 변경사항이 있는 경우에만 적용
	if (bPositionChanged)
	{
		SelectedActor->SetActorLocation(EditLocation);
		UE_LOG("Transform: Applied location (%.2f, %.2f, %.2f)", 
		       EditLocation.X, EditLocation.Y, EditLocation.Z);
	}
	
	if (bRotationChanged)
	{
		FQuat NewRotation = FQuat::MakeFromEuler(EditRotation);
		SelectedActor->SetActorRotation(NewRotation);
		UE_LOG("Transform: Applied rotation (%.1f, %.1f, %.1f)", 
		       EditRotation.X, EditRotation.Y, EditRotation.Z);
	}
	
	if (bScaleChanged)
	{
		SelectedActor->SetActorScale(EditScale);
		UE_LOG("Transform: Applied scale (%.2f, %.2f, %.2f)", 
		       EditScale.X, EditScale.Y, EditScale.Z);
	}
	
	// 플래그 리셋은 const 메서드에서 할 수 없으므로 PostProcess에서 처리
}

void UTargetActorTransformWidget::ResetChangeFlags()
{
	bPositionChanged = false;
	bRotationChanged = false;
	bScaleChanged = false;
}
