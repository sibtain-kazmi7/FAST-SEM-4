#include "shared.h"

void logq_init(LogQueue *q) {
    q->head = q->tail = q->count = 0;
    q->panel_next = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_mutex_init(&q->panel_mutex, NULL);
    sem_init(&q->sem_items, 0, 0);
    sem_init(&q->sem_space, 0, LOG_QUEUE_SIZE);
    memset(q->panel, 0, sizeof(q->panel));
}

void logq_destroy(LogQueue *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_mutex_destroy(&q->panel_mutex);
    sem_destroy(&q->sem_items);
    sem_destroy(&q->sem_space);
}

void logq_push(LogQueue *q, const char *fmt, ...) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 1000000;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    if (sem_timedwait(&q->sem_space, &ts) != 0) return;

    LogEntry e;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&q->mutex);
    q->entries[q->tail] = e;
    q->tail = (q->tail + 1) % LOG_QUEUE_SIZE;
    q->count++;
    pthread_mutex_unlock(&q->mutex);

    sem_post(&q->sem_items);
}

void *logger_thread(void *arg) {
    (void)arg;
    FILE *fp = fopen(LOG_FILE, "w");
    if (!fp) { perror("fopen log"); return NULL; }

    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 200000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

        if (sem_timedwait(&g_logq.sem_items, &ts) != 0) {
            if (atomic_load(&g_shutdown) && g_logq.count == 0) break;
            continue;
        }

        pthread_mutex_lock(&g_logq.mutex);
        LogEntry e = g_logq.entries[g_logq.head];
        g_logq.head  = (g_logq.head + 1) % LOG_QUEUE_SIZE;
        g_logq.count--;
        pthread_mutex_unlock(&g_logq.mutex);
        sem_post(&g_logq.sem_space);

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        struct tm *tm_info = localtime(&now.tv_sec);
        char tbuf[24];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);

        char line[320];
        snprintf(line, sizeof(line), "[%s.%03ld] %s",
                 tbuf, now.tv_nsec / 1000000L, e.msg);

        fprintf(fp, "%s\n", line);
        fflush(fp);

        /* store in panel ring for GUI — truncate safely */
        pthread_mutex_lock(&g_logq.panel_mutex);
        memset(g_logq.panel[g_logq.panel_next], 0, sizeof(g_logq.panel[0]));
        memcpy(g_logq.panel[g_logq.panel_next], line,
               strnlen(line, sizeof(g_logq.panel[0]) - 1));
        g_logq.panel_next = (g_logq.panel_next + 1) % LOG_PANEL_ROWS;
        pthread_mutex_unlock(&g_logq.panel_mutex);
    }

    fclose(fp);
    return NULL;
}
