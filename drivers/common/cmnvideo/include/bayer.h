#pragma once

#include <common.h>

uint32_t GetBayerAdjustedColour8(int x, int y, uint32_t colour);
uint32_t GetBayerAdjustedColour16(int x, int y, uint32_t colour);
uint8_t GetBayerAdjustedChannelForVeryHighQuality(int x, int y, uint16_t channel);