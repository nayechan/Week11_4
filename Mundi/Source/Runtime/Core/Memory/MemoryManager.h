#pragma once
#include <cstddef>
#include "UEContainer.h"

class FMemoryManager
{
public:
	// 인자 변수를 PascalCase로 변경
	static void* Allocate(SIZE_T Size, SIZE_T Alignment);
	static void  Deallocate(void* Ptr);

public:
	static uint32 TotalAllocationBytes;
	static uint32 TotalAllocationCount;
};