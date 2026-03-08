#pragma once

#ifndef PROD
#define PROD 0
#endif

#if !PROD
void kprintf(const char* format, ...);
#else
#define kprintf(...) do {} while(0)
#endif
