#pragma once

#include "AnimSequenceViewerState.h"

struct ID3D11Device;

// AnimSequence Viewer 탭별 상태 생성/삭제 헬퍼
// SkeletalMeshViewer의 Bootstrap 패턴을 따름
class AnimSequenceViewerBootstrap
{
public:
    // ViewerState 생성 (World, Viewport, ViewportClient, PreviewActor 모두 포함)
    static AnimSequenceViewerState* CreateViewerState(const char* Name, ID3D11Device* InDevice);

    // ViewerState 삭제 (모든 리소스 정리)
    static void DestroyViewerState(AnimSequenceViewerState*& State);
};
