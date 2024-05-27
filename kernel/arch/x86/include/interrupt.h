#pragma once

#include <stdbool.h>

bool x86IsReadyForIrqs(void);
void x86MakeReadyForIrqs(void);

void HandleNmi(void);