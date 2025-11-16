#pragma once
#include "Vector.h"

// FRect는 pch.h에 정의되어 있음

class SWindow
{
public:
    SWindow() = default;
    virtual ~SWindow() = default;

    // 기본 속성
    FRect Rect;


    // 마우스 호버 검사
    bool IsHover(FVector2D coord) const
    {
        return coord.X >= Rect.Min.X && coord.X <= Rect.Max.X &&
            coord.Y >= Rect.Min.Y && coord.Y <= Rect.Max.Y;
    }

    // 가상 함수들
    virtual void OnRender() {}
    virtual void OnUpdate(float DeltaSeconds) {}
    virtual void OnMouseMove(FVector2D MousePos) {}
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) {}
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) {}

    // 영역 설정
    void SetRect(const FRect& NewRect) { Rect = NewRect; }
    void SetRect(float MinX, float MinY, float MaxX, float MaxY)
    {
        Rect.Left = MinX; Rect.Top = MinY;
        Rect.Right = MaxX; Rect.Bottom = MaxY;
        Rect.UpdateMinMax();
    }

    FRect GetRect() const { return Rect; }
    float GetWidth() const { return Rect.Max.X - Rect.Min.X; }
    float GetHeight() const { return Rect.Max.Y - Rect.Min.Y; }
};