#include "mersenne.h"

mtwist_t *mtwist_new (int seed) = #0;
void mtwist_delete (mtwist_t *state) = #0;
void mtwist_seed (mtwist_t *state, int seed) = #0;
int mtwist_rand (mtwist_t *state) = #0;
float mtwist_rand_0_1 (mtwist_t *state) = #0;
float mtwist_rand_m1_1 (mtwist_t *state) = #0;
