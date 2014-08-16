/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Amanieu d'Antras

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Math functions not provided by the standard library */

#include "c_shared.h"

/* Constants for the Mersenne Twister */
#define N 624
#define M 397
#define UPPER_MASK 0x80000000
#define LOWER_MASK 0x7fffffff

/* Array holding a set of pseudorandom integers */
unsigned int state[N];

/* Pointer to the next integer to use:
   ptr == N means we need to generate a new set of integers
   ptr > N means the state has not be initialized */
unsigned int ptr = N + 1;

/******************************************************************************\
 Regenerate the generator's internal state. The psuedocode has been modified
 to remove branches and modulus operations within the loops.
\******************************************************************************/
static void regen_state(void)
{
        unsigned int i, tmp, magic[2] = {0, 0x9908b0dfUL};

        for (i = 0; i < N - M; i++) {
                tmp = (state[i] & UPPER_MASK) | (state[i + 1] & LOWER_MASK);
                state[i] = state[i + M] ^ (tmp >> 1) ^ magic[tmp & 1];
        }
        for (; i < N - 1; i++) {
                tmp = (state[i] & UPPER_MASK) | (state[i + 1] & LOWER_MASK);
                state[i] = state[i + M - N] ^ (tmp >> 1) ^ magic[tmp & 1];
        }
        tmp = (state[N - 1] & UPPER_MASK) | (state[0] & LOWER_MASK);
        state[N - 1] = state[M - 1] ^ (tmp >> 1) ^ magic[tmp & 1];
        ptr = 0;
}

/******************************************************************************\
 Initialize the generator from a seed.
\******************************************************************************/
void C_rand_seed(unsigned int seed)
{
        state[0] = seed;
        for (ptr = 1; ptr < N; ptr++)
                state[ptr] = (state[ptr - 1] ^ (state[ptr - 1] >> 30)) *
                             1812433253 + ptr;
        regen_state();
}

/******************************************************************************\
 Generate a random 31-bit signed integer from the internal state. Implements
 Makoto Matsumoto's Mersenne Twister random number generator:
 http://en.wikipedia.org/wiki/Mersenne_twister
\******************************************************************************/
int C_rand(void)
{
        unsigned int tmp;

        if (ptr > N)
                C_rand_seed((unsigned int)time(NULL));
        else if (ptr == N)
                regen_state();
        tmp = state[ptr++];
        tmp ^= tmp >> 11;
        tmp ^= (tmp << 7) & 0x9d2c5680UL;
        tmp ^= (tmp << 15) & 0xefc60000UL;
        tmp ^= tmp >> 18;
        return (int)(tmp & LOWER_MASK);
}

/******************************************************************************\
 Calculate the next power of two for any integer.
\******************************************************************************/
int C_next_pow2(int n)
{
        int p;

        if (n < 1)
                return 0;
        for (p = 1; p < n; p <<= 1);
        return p;
}

/******************************************************************************\
 Rotate [a] vector [prop] proportion around normal [n] toward another vector.
 If [a] is not in-plane with the normal vector, it will be made so. Only [n]
 needs to be normalized before calling this function.
\******************************************************************************/
c_vec3_t C_vec3_rotate_to(c_vec3_t a, c_vec3_t n, float prop, c_vec3_t b)
{
        c_vec3_t x, y;
        float angle, a_mag, bx, by;

        /* Save the input vector's length for later */
        a_mag = C_vec3_len(a);

        /* Create basis vectors */
        x = C_vec3_norm(C_vec3_in_plane(a, n));
        y = C_vec3_norm(C_vec3_cross(n, x));

        /* Compute the rotaton angle */
        b = C_vec3_norm(b);
        bx = C_vec3_dot(b, x);
        by = C_vec3_dot(b, y);
        if (!bx && !by)
                return a;
        angle = atan2f(by, bx) * prop;

        /* Apply rotation on basis vectors */
        return C_vec3_add(C_vec3_scalef(x, cosf(angle) * a_mag),
                          C_vec3_scalef(y, sinf(angle) * a_mag));
}

/******************************************************************************\
 Convenience function to ensure a value is within a range.
\******************************************************************************/
void C_limit_float(float *value, float min, float max)
{
        if (!value)
                return;
        if (*value < min)
                *value = min;
        if (*value > max)
                *value = max;
}

void C_limit_int(int *value, int min, int max)
{
        if (!value)
                return;
        if (*value < min)
                *value = min;
        if (*value > max)
                *value = max;
}

/******************************************************************************\
 Dice function for DnD nerds.
\******************************************************************************/
int C_roll_dice(int num, int sides)
{
        int i, total;

        for (total = 0, i = 0; i < num; i++)
                total += 1 + rand() % sides;
        return total;
}

