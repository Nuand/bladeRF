#ifndef _REL_ASSERT_H_
#define _REL_ASSERT_H_

#ifdef NDEBUG
#define NDEBUG_UNDEF
#endif
#undef NDEBUG
#include <assert.h>
#ifdef NDEBUG_UNDEF
#define NDEBUG
#undef NDEBUG_UNDEF
#endif

#endif /* _REL_ASSERT_H_ */