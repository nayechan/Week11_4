#include "pch.h"
#include "SkeletalMeshComponentCustomization.h"
#include "PropertyRenderer.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "ResourceManager.h"
#include "GlobalConsole.h"
#include "FbxManager.h"

IDetailCustomization* FSkeletalMeshComponentCustomization::MakeInstance()
{
	return new FSkeletalMeshComponentCustomization();
}

void FSkeletalMeshComponentCustomization::CustomizeDetails(UObject* Object)
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Object);
	if (!SkelComp)
	{
		UE_LOG("FSkeletalMeshComponentCustomization: Object is not USkeletalMeshComponent");
		return;
	}

	CurrentComponent = SkelComp;

	// Skeleton 포인터 추출
	TargetSkeleton = nullptr;
	if (SkelComp->GetSkeletalMesh())
	{
		// 우선순위 1: SkeletalMesh의 Skeleton
		TargetSkeleton = const_cast<FSkeleton*>(SkelComp->GetSkeletalMesh()->GetSkeleton());
	}
	else if (SkelComp->AnimToPlay)
	{
		// 우선순위 2: AnimToPlay의 Skeleton (SkeletalMesh 없을 때)
		TargetSkeleton = SkelComp->AnimToPlay->Skeleton;
	}

	// AnimToPlay 유효성 검사 (SkeletalMesh가 있을 때만 검증)
	if (SkelComp->AnimToPlay && TargetSkeleton)
	{
		// SkeletalMesh가 있는 경우에만 호환성 검사
		if (SkelComp->AnimToPlay->Skeleton != TargetSkeleton)
		{
			SkelComp->AnimToPlay = nullptr;
			UE_LOG("AnimToPlay cleared: incompatible Skeleton");
		}
	}

	// 필터 델리게이트 (포인터 비교)
	FOnShouldFilterAsset AssetFilterDelegate;
	AssetFilterDelegate.Bind([this](const FString& AssetPath) -> bool {
		return OnShouldFilterAnimAsset(AssetPath);
	});

	FOnShouldFilterProperty PropertyFilterDelegate;
	PropertyFilterDelegate.Bind([SkelComp](const FString& PropertyName) -> bool {
		// AnimationMode에 따라 해당하지 않는 프로퍼티 필터링
		if (PropertyName == "AnimToPlay")
		{
			return SkelComp->AnimationMode != EAnimationMode::AnimationSingleNode;
		}
		else if (PropertyName == "AnimClass")
		{
			return SkelComp->AnimationMode != EAnimationMode::AnimationNative;
		}
		else if (PropertyName == "AnimLuaScript")
		{
			return SkelComp->AnimationMode != EAnimationMode::AnimationLua;
		}
		return false;
	});

	// === 순서대로 렌더링: 부모 클래스 -> Skeletal Mesh -> Skeleton -> Animation ===

	// 1. 나머지 모든 카테고리 렌더링 (부모 클래스 프로퍼티 포함)
	TArray<FString> ExcludedCategories = {"Skeletal Mesh", "Animation"};
	UPropertyRenderer::RenderOtherCategories(SkelComp, ExcludedCategories, AssetFilterDelegate, PropertyFilterDelegate);

	// 2. Skeletal Mesh 카테고리 렌더링
	UPropertyRenderer::RenderCategoryOnly(SkelComp, "Skeletal Mesh", AssetFilterDelegate, PropertyFilterDelegate);

	// 3. Skeleton 섹션 렌더링
	RenderSkeletonSelector();

	// 4. Animation 카테고리 렌더링
	UPropertyRenderer::RenderCategoryOnly(SkelComp, "Animation", AssetFilterDelegate, PropertyFilterDelegate);
}

bool FSkeletalMeshComponentCustomization::OnShouldFilterAnimAsset(const FString& AssetPath)
{
	// 빈 경로 ("None")는 항상 표시
	if (AssetPath.empty())
	{
		return false;
	}

	// SkeletalMesh가 없으면 모든 애니메이션 표시 (Skeleton 드롭다운으로 변경한 경우에도)
	if (!CurrentComponent || !CurrentComponent->GetSkeletalMesh())
	{
		return false;
	}

	// TargetSkeleton이 없으면 모든 애니메이션 표시
	if (!TargetSkeleton)
	{
		return false;
	}

	// 애니메이션 로드
	UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AssetPath);
	if (!Anim)
	{
		return true;
	}

	// 포인터 비교 (SkeletalMesh가 있을 때만)
	return (Anim->Skeleton != TargetSkeleton);
}

void FSkeletalMeshComponentCustomization::RenderSkeletonSelector()
{
	if (!CurrentComponent) return;

	// AnimationMode가 AnimationSingleNode가 아니면 Skeleton 섹션을 렌더링하지 않음
	if (CurrentComponent->AnimationMode != EAnimationMode::AnimationSingleNode)
	{
		return;
	}

	// SkeletalMesh와 AnimToPlay가 모두 설정되지 않은 경우 Skeleton 섹션을 렌더링하지 않음
	if (!CurrentComponent->GetSkeletalMesh() && !CurrentComponent->AnimToPlay)
	{
		return;
	}

	ImGui::Separator();
	ImGui::Text("Skeleton");

	// 현재 Skeleton 이름
	FString CurrentSkeletonName = TargetSkeleton ? TargetSkeleton->Name : "None";

	// FFbxManager에서 Skeleton 목록 가져오기
	TArray<FString> AllSkeletonNames = FFbxManager::GetAllSkeletonNames();

	// ComboBox 아이템 배열
	TArray<const char*> Items;
	Items.Add("None");
	int CurrentIdx = 0;

	for (int i = 0; i < AllSkeletonNames.Num(); ++i)
	{
		Items.Add(AllSkeletonNames[i].c_str());
		if (AllSkeletonNames[i] == CurrentSkeletonName)
		{
			CurrentIdx = i + 1;
		}
	}

	// 드롭다운 렌더링
	ImGui::SetNextItemWidth(240);
	if (ImGui::Combo("Selected Skeleton", &CurrentIdx, Items.data(), static_cast<int>(Items.size())))
	{
		if (CurrentIdx == 0)
		{
			OnSkeletonChanged(nullptr);
		}
		else
		{
			FString SelectedName = AllSkeletonNames[CurrentIdx - 1];
			FSkeleton* NewSkeleton = FFbxManager::GetSkeletonByName(SelectedName);
			OnSkeletonChanged(NewSkeleton);
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Change the Skeleton for both SkeletalMesh and AnimToPlay");
		ImGui::EndTooltip();
	}
}

void FSkeletalMeshComponentCustomization::OnSkeletonChanged(FSkeleton* NewSkeleton)
{
	if (!CurrentComponent) return;

	USkeletalMesh* Mesh = CurrentComponent->GetSkeletalMesh();
	UAnimSequence* Anim = CurrentComponent->AnimToPlay;

	// 1. SkeletalMesh의 Skeleton 교체
	if (Mesh)
	{
		Mesh->SetSkeleton(NewSkeleton);
		Mesh->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";
		UE_LOG("Changed SkeletalMesh Skeleton to: %s",
			   NewSkeleton ? NewSkeleton->Name.c_str() : "None");
	}

	// 2. AnimToPlay의 Skeleton 교체
	if (Anim)
	{
		Anim->Skeleton = NewSkeleton;
		Anim->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";
		UE_LOG("Changed Animation Skeleton to: %s",
			   NewSkeleton ? NewSkeleton->Name.c_str() : "None");
	}

	// 3. TargetSkeleton 업데이트
	TargetSkeleton = NewSkeleton;

	// 4. AnimToPlay 호환성 확인
	if (Anim && NewSkeleton && Anim->Skeleton != NewSkeleton)
	{
		CurrentComponent->AnimToPlay = nullptr;
		UE_LOG("AnimToPlay cleared: incompatible with new Skeleton");
	}

	// 5. Skeleton 변경사항 저장
	// SkeletalMesh의 Skeleton 오버라이드 저장
	if (Mesh && !Mesh->GetFilePath().empty())
	{
		FString OverridePath = Mesh->GetFilePath() + ".skeleton";
		if (Mesh->SaveOverrideData(OverridePath))
		{
			UE_LOG("Saved mesh skeleton override: %s", OverridePath.c_str());
		}
	}

	// AnimToPlay의 Skeleton 오버라이드 저장
	if (Anim && !Anim->GetFilePath().empty())
	{
		FString OverridePath = Anim->GetFilePath() + ".skeleton";
		if (Anim->SaveOverrideData(OverridePath))
		{
			UE_LOG("Saved animation skeleton override: %s", OverridePath.c_str());
		}
	}
}
