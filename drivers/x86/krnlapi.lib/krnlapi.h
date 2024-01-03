#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <_syscallnum.h>
#include <sys/types.h>

int _system_call(int call, size_t a, size_t b, size_t c, size_t d, size_t e);