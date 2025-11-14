#include "pch.h"
#include "SSplitterV.h"

SSplitterV::~SSplitterV()
{
  /*  delete SideLT;
    delete SideRB;
    SideLT = nullptr;
    SideRB = nullptr;*/
}
void SSplitterV::UpdateDrag(FVector2D MousePos)
{
    if (!bIsDragging) return;

    // 마우스 Y 좌표를 비율로 변환
    float NewSplitY = MousePos.Y;
    float MinY = Rect.Min.Y + SplitterThickness;
    float MaxY = Rect.Max.Y - SplitterThickness;

    // 경계 제한
    NewSplitY = FMath::Clamp(NewSplitY, MinY, MaxY);

    // 새 비율 계산
    float NewRatio = (NewSplitY - Rect.Min.Y) / GetHeight();
    SetSplitRatio(NewRatio);
}

void SSplitterV::UpdateChildRects()
{
    if (!SideLT && !SideRB) return;

    // SideRB가 없으면 SideLT가 전체 영역 사용
    if (SideLT && !SideRB)
    {
        SideLT->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
        return;
    }

    // SideLT가 없으면 SideRB가 전체 영역 사용
    if (!SideLT && SideRB)
    {
        SideRB->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
        return;
    }

    // 둘 다 있으면 비율대로 분할
    float SplitY = Rect.Min.Y + (GetHeight() * SplitRatio);

    // Top 영역
    SideLT->SetRect(
        Rect.Min.X, Rect.Min.Y,
        Rect.Max.X, SplitY - SplitterThickness / 2
    );

    // Bottom 영역
    SideRB->SetRect(
        Rect.Min.X, SplitY + SplitterThickness / 2,
        Rect.Max.X, Rect.Max.Y
    );
}

FRect SSplitterV::GetSplitterRect() const
{
    float SplitY = Rect.Min.Y + (GetHeight() * SplitRatio);
    return FRect(
        Rect.Min.X, SplitY - SplitterThickness / 2,
        Rect.Max.X, SplitY + SplitterThickness / 2
    );
}
