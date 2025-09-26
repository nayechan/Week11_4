#pragma once
#include "GizmoArrowComponent.h"
class UGizmoScaleComponent :
    public UGizmoArrowComponent
{
public:
    UGizmoScaleComponent();

protected:
    ~UGizmoScaleComponent() override;

public:
    DECLARE_CLASS(UGizmoScaleComponent, UGizmoArrowComponent)
};

