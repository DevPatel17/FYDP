// #ifndef RING_BUFF_H
// #define RING_BUFF_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "freertos/task.h"
#include "freertos/semphr.h"
// #include "freertos.h"

#define BUFFER_SIZE 10

typedef struct {
    float buffer[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t size;

    SemaphoreHandle_t mutex;
    SemaphoreHandle_t not_full;
    SemaphoreHandle_t not_empty;
} RingBuffer;


// Make da jaunt
void ring_buffer_init(RingBuffer *rb);

// Get rid of da jaunt
void ring_buffer_destructor(RingBuffer *rb);

// Give da jaunt a float to store in da jaunt
void ring_buffer_write(RingBuffer *rb, float val);

// Get the jaunt from the jaunt
float ring_buffer_read(RingBuffer *rb);

// #endif // RING_BUFF_H