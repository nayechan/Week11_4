#include "pch.h"
#include "AnimNode_SequencePlayer.h"
#include "AnimSequence.h"
#include "AnimationRuntime.h"
#include "VertexData.h"

// ========================================
// UAnimNode_SequencePlayer 구현
// ========================================

// 모든 메서드는 헤더에 inline으로 구현됨
// 이 cpp 파일은 다음 용도로 유지:
// 1. 컴파일 단위 분리 (빌드 시간 최적화)
// 2. 필요한 헤더 include (AnimSequence, AnimationRuntime 등)
// 3. 리플렉션 시스템 연동
