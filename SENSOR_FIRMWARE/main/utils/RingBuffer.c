#include "RingBuffer.h"

/// TODO: change this structure to always output the LATEST data.


void ring_buffer_init(RingBuffer *rb) {

    rb->head = 0, rb->tail = 0, rb->size = 0;

    rb->mutex = xSemaphoreCreateMutex();
    rb->not_full = xSemaphoreCreateCounting(BUFFER_SIZE, BUFFER_SIZE);
    rb->not_empty = xSemaphoreCreateCounting(BUFFER_SIZE, 0);
}

void ring_buffer_destructor(RingBuffer *rb) {
    vSemaphoreDelete(rb->mutex);
    vSemaphoreDelete(rb->not_empty);
    vSemaphoreDelete(rb->not_full);
}

void ring_buffer_write(RingBuffer *rb, float val) {

    xSemaphoreTake(rb->not_full, portMAX_DELAY);
    xSemaphoreTake(rb->mutex, portMAX_DELAY);


    rb->buffer[rb->head] = val;
    rb->head = (rb->head + 1) % BUFFER_SIZE;
    rb->size++;

    xSemaphoreGive(rb->mutex);
    xSemaphoreGive(rb->not_empty);
}

float ring_buffer_read(RingBuffer *rb) {
    xSemaphoreTake(rb->not_empty, portMAX_DELAY);
    xSemaphoreTake(rb->mutex, portMAX_DELAY);

    float value = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % BUFFER_SIZE;
    rb->size--;

    xSemaphoreGive(rb->mutex);
    xSemaphoreGive(rb->not_full);

    return value;
}