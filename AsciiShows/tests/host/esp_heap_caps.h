#pragma once
#include <stdlib.h>
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline bool hostAllocationFails=false;
inline void *heap_caps_malloc(size_t bytes,int) { return hostAllocationFails?nullptr:malloc(bytes); }
inline size_t heap_caps_get_free_size(int) { return 0; }
