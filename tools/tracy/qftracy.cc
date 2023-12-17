// this works around a limitation with automake and source files that are
// not part of the distribution
#ifdef HAVE_TRACY
#include "tracy/public/TracyClient.cpp"
#endif
