#include "pch.h"
#include "AnimNode.h"
#include "AnimInstance.h"
#include "VertexData.h"

// ========================================
// UAnimNode 구현
// ========================================

// 모든 핵심 메서드는 헤더에 inline으로 구현됨
// 이 cpp 파일은 다음 용도로 유지:
// 1. 컴파일 단위 분리 (빌드 시간 최적화)
// 2. 향후 확장 (가상 소멸자 등)
// 3. 리플렉션 시스템 연동

// NOTE: Initialize()와 Update()는 헤더에 기본 구현 제공
// Evaluate()는 순수 가상 함수로 하위 클래스에서 반드시 구현
