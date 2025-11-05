#pragma once
#include  "PostProcessing.h"
#include <D3D11RHI.h>

class FGammaPass final : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

};