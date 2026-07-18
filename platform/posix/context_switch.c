#include "microposix/kernel/thread.h"
#include <stdlib.h>

void mp_context_switch(mp_thread_t *from, mp_thread_t *to) {
    (void)from;
    (void)to;
    // POSIX context switch - just a placeholder for testing
}
