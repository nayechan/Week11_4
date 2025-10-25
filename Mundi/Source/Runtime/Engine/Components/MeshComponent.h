#pragma once
#include "PrimitiveComponent.h"

class UShader;

class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UMeshComponent)

    bool IsCastShadows() { return bCastShadows; }

private:
    bool bCastShadows = true;   // TODO: 프로퍼티로 추가 필요
};