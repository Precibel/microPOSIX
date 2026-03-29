#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"

// Mock architecture-specific functions for testing
void mp_hal_cpu_trigger_context_switch(void) {
    // Mock
}

void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    (void)tcb; (void)func; (void)arg;
}

void *dummy_thread_func(void *arg) {
    (void)arg;
    return NULL;
}

void test_scheduler_init(void) {
    mp_sched_init();
    mp_tcb_t *current = mp_sched_get_current_thread();
    assert(current != NULL);
    assert(strcmp(current->name, "idle") == 0);
    printf("test_scheduler_init passed\n");
}

void test_thread_creation(void) {
    mp_sched_init();
    mp_thread_attr_t attr = {.name = "test_thread", .priority = 10, .stack_size = 512};
    mp_thread_id_t tid = mp_thread_create(dummy_thread_func, NULL, &attr);
    assert(tid > 0);
    
    mp_sched_reschedule();
    mp_tcb_t *next = mp_sched_get_next_thread();
    assert(next != NULL);
    assert(strcmp(next->name, "test_thread") == 0);
    printf("test_thread_creation passed\n");
}

void test_priority_preemption(void) {
    mp_sched_init();
    
    mp_thread_attr_t attr_low = {.name = "low", .priority = 5};
    mp_thread_create(dummy_thread_func, NULL, &attr_low);
    
    mp_thread_attr_t attr_high = {.name = "high", .priority = 15};
    mp_thread_create(dummy_thread_func, NULL, &attr_high);
    
    mp_sched_reschedule();
    mp_tcb_t *next = mp_sched_get_next_thread();
    assert(next != NULL);
    assert(strcmp(next->name, "high") == 0);
    printf("test_priority_preemption passed\n");
}

int main(void) {
    test_scheduler_init();
    test_thread_creation();
    test_priority_preemption();
    printf("All scheduler tests passed!\n");
    return 0;
}
