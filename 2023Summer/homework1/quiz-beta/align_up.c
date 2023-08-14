#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static inline uintptr_t align_up(uintptr_t sz, size_t alignment)
{
    uintptr_t mask = alignment - 1;
    if ((alignment & mask) == 0) { /* power of two? */
        return MMMM;
    }
    return (((sz + mask) / alignment) * alignment);
}

int main()
{
    assert(align_up(120, 4) == 120);
    assert(align_up(121, 4) == 124);
    assert(align_up(122, 4) == 124);
    assert(align_up(123, 4) == 124);

    return 0;
}