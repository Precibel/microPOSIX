/**
 * ESP32 Context Switch Implementation
 * 
 * ESP32 uses FreeRTOS as its underlying RTOS, so we need to integrate
 * with FreeRTOS's scheduling system rather than implementing our own.
 * 
 * This file provides the interface between microPOSIX and FreeRTOS.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "mp_context_switch";

// FreeRTOS task handles for microPOSIX threads
// We need to map microPOSIX TCBs to FreeRTOS tasks
#define MAX_MICROPOSIX_THREADS 32

static TaskHandle_t mp_freertos_tasks[MAX_MICROPOSIX_THREADS] = {0};
static mp_tcb_t *mp_freertos_tcb_map[MAX_MICROPOSIX_THREADS] = {0};

// Mutex for thread map access
static SemaphoreHandle_t mp_thread_map_mutex = NULL;

// Initialize the context switch system for ESP32
void mp_hal_esp32_context_switch_init(void) {
    if (mp_thread_map_mutex == NULL) {
        mp_thread_map_mutex = xSemaphoreCreateMutex();
        ESP_LOGI(TAG, "Context switch system initialized");
    }
}

// Create a FreeRTOS task for a microPOSIX thread
int mp_hal_esp32_create_freertos_task(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    if (mp_thread_map_mutex == NULL) {
        mp_hal_esp32_context_switch_init();
    }
    
    // Find a free slot in the task map
    int free_slot = -1;
    for (int i = 0; i < MAX_MICROPOSIX_THREADS; i++) {
        if (mp_freertos_tasks[i] == NULL) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) {
        ESP_LOGE(TAG, "No free slots for new thread");
        return -1;
    }
    
    // Create FreeRTOS task
    // Stack size: use microPOSIX stack size or minimum FreeRTOS stack
    uint32_t stack_size_words = (tcb->stack_size + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    if (stack_size_words < configMINIMAL_STACK_SIZE / sizeof(StackType_t)) {
        stack_size_words = configMINIMAL_STACK_SIZE / sizeof(StackType_t);
    }
    
    // Priority mapping: microPOSIX priority (0-31) to FreeRTOS priority
    // FreeRTOS priorities are inverted (0 = lowest, configMAX_PRIORITIES-1 = highest)
    UBaseType_t freertos_priority = tcb->priority;
    if (freertos_priority >= configMAX_PRIORITIES) {
        freertos_priority = configMAX_PRIORITIES - 1;
    }
    
    TaskHandle_t task_handle = NULL;
    BaseType_t result = xTaskCreate(
        (TaskFunction_t)func,   // Task function
        tcb->name,               // Task name
        stack_size_words,        // Stack size in words
        arg,                     // Task parameters
        freertos_priority,       // Priority
        &task_handle             // Task handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS task for thread %s", tcb->name);
        return -1;
    }
    
    // Store the mapping
    xSemaphoreTake(mp_thread_map_mutex, portMAX_DELAY);
    mp_freertos_tasks[free_slot] = task_handle;
    mp_freertos_tcb_map[free_slot] = tcb;
    tcb->platform_data = (void *)(intptr_t)free_slot;  // Store slot index
    xSemaphoreGive(mp_thread_map_mutex);
    
    ESP_LOGD(TAG, "Created FreeRTOS task for thread %s (priority %d)", tcb->name, tcb->priority);
    return 0;
}

// Destroy a FreeRTOS task for a microPOSIX thread
void mp_hal_esp32_destroy_freertos_task(mp_tcb_t *tcb) {
    if (tcb->platform_data == NULL) {
        return;
    }
    
    int slot = (intptr_t)tcb->platform_data;
    if (slot < 0 || slot >= MAX_MICROPOSIX_THREADS) {
        return;
    }
    
    xSemaphoreTake(mp_thread_map_mutex, portMAX_DELAY);
    TaskHandle_t task_handle = mp_freertos_tasks[slot];
    mp_freertos_tasks[slot] = NULL;
    mp_freertos_tcb_map[slot] = NULL;
    xSemaphoreGive(mp_thread_map_mutex);
    
    if (task_handle != NULL) {
        vTaskDelete(task_handle);
        ESP_LOGD(TAG, "Deleted FreeRTOS task for thread %s", tcb->name);
    }
    
    tcb->platform_data = NULL;
}

// Initialize stack for ESP32 (not used directly, FreeRTOS handles this)
void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    // On ESP32, FreeRTOS handles stack initialization
    // We just need to create the FreeRTOS task
    mp_hal_esp32_create_freertos_task(tcb, func, arg);
}

// Trigger context switch (on ESP32, this is handled by FreeRTOS)
void mp_hal_cpu_trigger_context_switch(void) {
    // On ESP32 with FreeRTOS, we can trigger a context switch
    // by calling taskYIELD() or portYIELD()
    taskYIELD();
}

// Enter critical section
uint32_t mp_hal_cpu_enter_critical(void) {
    // Use FreeRTOS critical section
    taskENTER_CRITICAL();
    return 0;  // Return value not used on ESP32
}

// Exit critical section
void mp_hal_cpu_exit_critical(uint32_t status) {
    (void)status;
    taskEXIT_CRITICAL();
}

// Get current thread's TCB
mp_tcb_t *mp_hal_esp32_get_current_tcb(void) {
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    
    if (mp_thread_map_mutex == NULL) {
        return NULL;
    }
    
    xSemaphoreTake(mp_thread_map_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_MICROPOSIX_THREADS; i++) {
        if (mp_freertos_tasks[i] == current_task) {
            mp_tcb_t *tcb = mp_freertos_tcb_map[i];
            xSemaphoreGive(mp_thread_map_mutex);
            return tcb;
        }
    }
    xSemaphoreGive(mp_thread_map_mutex);
    return NULL;
}

// Suspend a FreeRTOS task
void mp_hal_esp32_suspend_task(mp_tcb_t *tcb) {
    if (tcb->platform_data == NULL) {
        return;
    }
    
    int slot = (intptr_t)tcb->platform_data;
    if (slot < 0 || slot >= MAX_MICROPOSIX_THREADS) {
        return;
    }
    
    TaskHandle_t task_handle = mp_freertos_tasks[slot];
    if (task_handle != NULL) {
        vTaskSuspend(task_handle);
    }
}

// Resume a FreeRTOS task
void mp_hal_esp32_resume_task(mp_tcb_t *tcb) {
    if (tcb->platform_data == NULL) {
        return;
    }
    
    int slot = (intptr_t)tcb->platform_data;
    if (slot < 0 || slot >= MAX_MICROPOSIX_THREADS) {
        return;
    }
    
    TaskHandle_t task_handle = mp_freertos_tasks[slot];
    if (task_handle != NULL) {
        vTaskResume(task_handle);
    }
}

// Delay for a number of milliseconds (uses FreeRTOS delay)
void mp_hal_esp32_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Get current tick count
uint32_t mp_hal_esp32_get_tick_count(void) {
    return xTaskGetTickCount();
}

// Get current tick count from ISR
uint32_t mp_hal_esp32_get_tick_count_from_isr(void) {
    return xTaskGetTickCountFromISR();
}

// Check if we're in an ISR
bool mp_hal_esp32_is_in_isr(void) {
    return xPortInIsrContext();
}
