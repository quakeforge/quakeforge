#ifndef __ruamoko_mersenne_h
#define __ruamoko_mersenne_h

typedef struct _mtwist_t mtwist_t;

mtwist_t *mtwist_new (int seed);
void mtwist_delete (mtwist_t *state);
void mtwist_seed (mtwist_t *state, int seed);
int mtwist_rand (mtwist_t *state);
// includes 0, does not include 1
float mtwist_rand_0_1 (mtwist_t *state);
// does not include either -1 or 1
float mtwist_rand_m1_1 (mtwist_t *state);

#endif//__ruamoko_mersenne_h
