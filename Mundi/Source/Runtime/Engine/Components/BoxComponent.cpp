#include "pch.h"
#include "BoxComponent.h"

IMPLEMENT_CLASS(UBoxComponent)

BEGIN_PROPERTIES(UBoxComponent)
MARK_AS_COMPONENT("박스 충돌 컴포넌트", "박스 모양의 충돌체를 생성하는 컴포넌트입니다.")
	ADD_PROPERTY(FVector, BoxExtent, "BoxExtent", true, "박스 충돌체의 크기입니다.")
END_PROPERTIES()

UBoxComponent::UBoxComponent()
{
	BoxExtent = WorldAABB.GetHalfExtent(); 
}

void UBoxComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	BoxExtent = WorldAABB.GetHalfExtent();
	 
}

void UBoxComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UBoxComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Box;
	Out.Box.BoxExtent = BoxExtent;
}

void UBoxComponent::RenderDebugVolume(URenderer* Renderer) const
{
	if (!bShapeIsVisible) return;
	if (GetOwner() && (GetOwner()->GetWorld()->bPie))
	{
		if (bShapeHiddenInGame)
			return;
	}


	const FVector Extent = BoxExtent;
	const FTransform WorldTransform = GetWorldTransform();

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	FVector local[8] = {
		{-Extent.X, -Extent.Y, -Extent.Z}, {+Extent.X, -Extent.Y, -Extent.Z},
		{-Extent.X, +Extent.Y, -Extent.Z}, {+Extent.X, +Extent.Y, -Extent.Z},
		{-Extent.X, -Extent.Y, +Extent.Z}, {+Extent.X, -Extent.Y, +Extent.Z},
		{-Extent.X, +Extent.Y, +Extent.Z}, {+Extent.X, +Extent.Y, +Extent.Z},
	};

	//월드 space로 변환
	FVector WorldSpace[8]; 
	for (int i = 0; i < 8; i++)
	{ 
		WorldSpace[i] = WorldTransform.TransformPosition(local[i]);
	}

	static const int Edge[12][2] = {
		{0,1},{1,3},{3,2},{2,0}, // bottom
		{4,5},{5,7},{7,6},{6,4}, // top
		{0,4},{1,5},{2,6},{3,7}  // verticals
	};
	for (int i = 0; i < 12; ++i)
	{
		StartPoints.Add(WorldSpace[Edge[i][0]]);
		EndPoints.Add(WorldSpace[Edge[i][1]]);
		Colors.Add(ShapeColor); // 동일 색으로 라인 렌더
	}
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

