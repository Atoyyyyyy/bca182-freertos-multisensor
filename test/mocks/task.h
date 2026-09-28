#ifndef TEST_MOCK_TASK_H
#define TEST_MOCK_TASK_H

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(x) \
    ((TickType_t)(((uint32_t)(x) * 20U) / 1000U))

#endif