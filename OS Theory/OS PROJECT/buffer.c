#include "shared.h"

void buffer_init(Buffer *b, int size) {
    b->data  = (int *)malloc(sizeof(int) * size);
    if (!b->data) { perror("malloc"); exit(EXIT_FAILURE); }
    b->size = size; b->head = 0; b->tail = 0; b->count = 0;
    pthread_mutex_init(&b->mutex,      NULL);
    pthread_mutex_init(&b->fair_mutex, NULL);
    pthread_cond_init (&b->fair_cond,  NULL);
    sem_init(&b->sem_empty, 0, size);
    sem_init(&b->sem_full,  0, 0);
    atomic_store(&b->ticket_next, 0);
    atomic_store(&b->ticket_now,  0);
}

void buffer_destroy(Buffer *b) {
    free(b->data);
    pthread_mutex_destroy(&b->mutex);
    pthread_mutex_destroy(&b->fair_mutex);
    pthread_cond_destroy (&b->fair_cond);
    sem_destroy(&b->sem_empty);
    sem_destroy(&b->sem_full);
}

static void fair_acquire(Buffer *b) {
    if (!g_cfg.fair) return;
    int my_ticket = atomic_fetch_add(&b->ticket_next, 1);
    pthread_mutex_lock(&b->fair_mutex);
    while (atomic_load(&b->ticket_now) != my_ticket)
        pthread_cond_wait(&b->fair_cond, &b->fair_mutex);
    pthread_mutex_unlock(&b->fair_mutex);
}

static void fair_release(Buffer *b) {
    if (!g_cfg.fair) return;
    atomic_fetch_add(&b->ticket_now, 1);
    pthread_mutex_lock(&b->fair_mutex);
    pthread_cond_broadcast(&b->fair_cond);
    pthread_mutex_unlock(&b->fair_mutex);
}

void buffer_produce(Buffer *b, int item, ThreadStats *st) {
    double t0 = now_ms();
    fair_acquire(b);
    sem_wait(&b->sem_empty);
    st->total_wait_ms += now_ms() - t0;

    pthread_mutex_lock(&b->mutex);
    b->data[b->tail] = item;
    b->tail = (b->tail + 1) % b->size;
    b->count++;
    int snap = b->count;
    pthread_mutex_unlock(&b->mutex);

    sem_post(&b->sem_full);
    fair_release(b);

    st->ops++;
    atomic_fetch_add(&g_total_produced, 1);
    logq_push(&g_logq, "[PRODUCE] Producer-%d  item=%-6d  buf=%d/%d",
              st->id, item, snap, b->size);
}

int buffer_consume(Buffer *b, ThreadStats *st) {
    double t0 = now_ms();
    fair_acquire(b);
    sem_wait(&b->sem_full);
    st->total_wait_ms += now_ms() - t0;

    pthread_mutex_lock(&b->mutex);
    int item = b->data[b->head];
    b->head  = (b->head + 1) % b->size;
    b->count--;
    int snap = b->count;
    pthread_mutex_unlock(&b->mutex);

    sem_post(&b->sem_empty);
    fair_release(b);

    st->ops++;
    atomic_fetch_add(&g_total_consumed, 1);
    logq_push(&g_logq, "[CONSUME] Consumer-%d  item=%-6d  buf=%d/%d",
              st->id, item, snap, b->size);
    return item;
}
