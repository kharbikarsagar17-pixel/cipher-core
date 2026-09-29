/*
 * Psi Module - Pseudo-Random Number Generation & Entropy
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define PSI_VERSION "1.0.0"

typedef struct {
    uint64_t state[2];
} Xoroshiro128;

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t psi_next(Xoroshiro128 *rng) {
    const uint64_t s0 = rng->state[0];
    uint64_t s1 = rng->state[1];
    const uint64_t result = rotl(s0 + s1, 17) + s0;

    s1 ^= s0;
    rng->state[0] = rotl(s0, 49) ^ s1 ^ (s1 << 21);
    rng->state[1] = rotl(s1, 28);

    return result;
}

static void psi_seed(Xoroshiro128 *rng, uint64_t seed1, uint64_t seed2) {
    rng->state[0] = seed1 ? seed1 : 0x123456789ABCDEF0ULL;
    rng->state[1] = seed2 ? seed2 : 0xFEDCBA9876543210ULL;
}

static void psi_init(void) {
    printf("Psi PRNG & entropy module initialized v%s\n", PSI_VERSION);
}
