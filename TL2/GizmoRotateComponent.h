#pragma once
#include "GizmoArrowComponent.h"
class UGizmoRotateComponent :public UGizmoArrowComponent
{
public:
    UGizmoRotateComponent();

protected:
    ~UGizmoRotateComponent() override;

public:
    DECLARE_CLASS(UGizmoRotateComponent, UGizmoArrowComponent)
};

