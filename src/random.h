
// random.h

#ifndef RANDOM_H
#define RANDOM_H

// includes

#include "util.h"
#include <cstdint>

// constants

const int RandomNb = 781;

// From MMIX by Donald Knuth.
constexpr uint64_t RandomMultiplier = 6364136223846793005;
constexpr uint64_t RandomOffset = 1442695040888963407;
inline uint64_t RandomLinearCongruential(uint64_t x) { return x * RandomMultiplier + RandomOffset; }

// macros

//#define RANDOM_64(n) (RandomLinearCongruential(n))


#endif // !defined RANDOM_H

// end of random.h

