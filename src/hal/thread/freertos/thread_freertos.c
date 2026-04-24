/*
 *  thread_freertos.c
 *
 *  FreeRTOS threading/synchronization adapter for lib60870 HAL.
 */

#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "hal_thread.h"
#include "lib_memory.h"

#ifndef LIB60870_THREAD_STACK_WORDS
    #define LIB60870_THREAD_STACK_WORDS (1024)
#endif

#ifndef LIB60870_THREAD_PRIORITY
    #define LIB60870_THREAD_PRIORITY (tskIDLE_PRIORITY + 1U)
#endif

struct sThread
{
    ThreadExecutionFunction function;
    void* parameter;
    TaskHandle_t taskHandle;
    SemaphoreHandle_t finishedSem;
    int state;
    bool autodestroy;
};

static void
threadEntry(void* parameter)
{
    Thread thread = (Thread) parameter;

    if ((thread != NULL) && (thread->function != NULL))
        (void) thread->function(thread->parameter);

    if (thread != NULL)
    {
        if (thread->autodestroy)
        {
            if (thread->finishedSem != NULL)
                vSemaphoreDelete(thread->finishedSem);

            GLOBAL_FREEMEM(thread);
        }
        else
        {
            thread->state = 0;

            if (thread->finishedSem != NULL)
                (void) xSemaphoreGive(thread->finishedSem);
        }
    }

    vTaskDelete(NULL);
}

Thread
Thread_create(ThreadExecutionFunction function, void* parameter, bool autodestroy)
{
    Thread thread = (Thread) GLOBAL_MALLOC(sizeof(struct sThread));

    if (thread == NULL)
        return NULL;

    thread->function = function;
    thread->parameter = parameter;
    thread->taskHandle = NULL;
    thread->state = 0;
    thread->autodestroy = autodestroy;
    thread->finishedSem = NULL;

    if (!autodestroy)
    {
        thread->finishedSem = xSemaphoreCreateBinary();

        if (thread->finishedSem == NULL)
        {
            GLOBAL_FREEMEM(thread);
            return NULL;
        }
    }

    return thread;
}

void Thread_start(Thread thread)
{
    if ((thread == NULL) || (thread->state == 1))
        return;

    BaseType_t status = xTaskCreate(threadEntry,
                                    "lib60870",
                                    (configSTACK_DEPTH_TYPE) LIB60870_THREAD_STACK_WORDS,
                                    thread,
                                    (UBaseType_t) LIB60870_THREAD_PRIORITY,
                                    &thread->taskHandle);

    if (status == pdPASS)
        thread->state = 1;
}

void Thread_destroy(Thread thread)
{
    if (thread == NULL)
        return;

    if (thread->autodestroy)
    {
        /*
         * Auto-destroy thread object is freed by threadEntry() after task exit.
         * But if task was never started (or start failed), we must free here.
         */
        if (thread->state == 0)
            GLOBAL_FREEMEM(thread);

        return;
    }

    if ((thread->state == 1) && (thread->finishedSem != NULL))
        (void) xSemaphoreTake(thread->finishedSem, portMAX_DELAY);

    if (thread->finishedSem != NULL)
        vSemaphoreDelete(thread->finishedSem);

    GLOBAL_FREEMEM(thread);
}

void Thread_sleep(int millies)
{
    if (millies <= 0)
    {
        taskYIELD();
        return;
    }

    vTaskDelay(pdMS_TO_TICKS((TickType_t) millies));
}

Semaphore
Semaphore_create(int initialValue)
{
    if (initialValue < 0)
        initialValue = 0;

    UBaseType_t init = (UBaseType_t) initialValue;
    UBaseType_t max = (UBaseType_t) 0xFFFF;

    if (init > max)
        init = max;

    return (Semaphore) xSemaphoreCreateCounting(max, init);
}

void Semaphore_wait(Semaphore self)
{
    if (self != NULL)
        (void) xSemaphoreTake((SemaphoreHandle_t) self, portMAX_DELAY);
}

void Semaphore_post(Semaphore self)
{
    if (self != NULL)
        (void) xSemaphoreGive((SemaphoreHandle_t) self);
}

void Semaphore_destroy(Semaphore self)
{
    if (self != NULL)
        vSemaphoreDelete((SemaphoreHandle_t) self);
}
