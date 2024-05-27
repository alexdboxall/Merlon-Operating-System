#pragma once

size_t OsGetTotalMemoryKilobytes(void);
size_t OsGetFreeMemoryKilobytes(void);

void OsGetVersion(int* major, int* minor, char* string, int length);
