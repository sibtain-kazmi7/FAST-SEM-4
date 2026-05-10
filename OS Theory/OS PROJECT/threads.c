#include "shared.h"

void *producer_thread(void *arg) {
    ThreadStats *st = (ThreadStats *)arg;
    int item = st->id * 1000;

    while (!atomic_load(&g_shutdown)) {
        buffer_produce(&g_buf, item++, st);
        sleep_ms(st->rate_ms);
    }

    logq_push(&g_logq, "[INFO ] Producer-%d exiting  ops=%ld", st->id, st->ops);
    return NULL;
}

void *consumer_thread(void *arg) {
    ThreadStats *st = (ThreadStats *)arg;

    while (1) {
        /* if shutdown flagged, only exit when buffer is empty */
        if (atomic_load(&g_shutdown)) {
            pthread_mutex_lock(&g_buf.mutex);
            int empty = (g_buf.count == 0);
            pthread_mutex_unlock(&g_buf.mutex);
            if (empty) break;
        }

        /* timed wait so consumers notice shutdown without busy-waiting */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 300000000L;   /* 300 ms */
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

        if (sem_timedwait(&g_buf.sem_full, &ts) != 0) continue;

        /* re-post so buffer_consume's own sem_wait sees it */
        sem_post(&g_buf.sem_full);

        buffer_consume(&g_buf, st);
        sleep_ms(st->rate_ms);
    }

    logq_push(&g_logq, "[INFO ] Consumer-%d exiting  ops=%ld", st->id, st->ops);
    return NULL;
}
