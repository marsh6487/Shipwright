#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t GlobalOutdoorRain_GetRenderColor(uint8_t* red, uint8_t* green, uint8_t* blue);
void GlobalOutdoorRain_NotifyNativeRainActive(int32_t active, int32_t thunderActive);

#ifdef __cplusplus
}
#endif
