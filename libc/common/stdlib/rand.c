
#include <stdint.h>

static uint64_t rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 164603309694725029ULL + 14738995463583502973ULL;
    return (rand_seed >> 33) & 0x7FFFFFFF;
}

void srand(unsigned int seed) {
    rand_seed = seed;
}