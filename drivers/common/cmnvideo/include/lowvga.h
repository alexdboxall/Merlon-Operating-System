#pragma once

#include <common.h>

int Convert9BitToVga16(int ninebit);
int Convert12BitToVga256(int twelvebit);
bool IsExactVgaMatch(uint32_t colour, bool palette256);