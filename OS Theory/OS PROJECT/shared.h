#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <stdatomic.h>
#include <ncurses.h>
#include <limits.h>

#define MAX_BUFFER      256
#define MAX_THREADS     32
#define LOG_QUEUE_SIZE  512
#define LOG_FILE        "logs/activity.log"
#define LOG_PANEL_ROWS  10   /* lines shown in GUI log panel */

/* ── Shared circular buffer ─────────────────────────────────────────────── */
typedef struct {
    int     *data;
    int      size;
    int      head;
    int      tail;
    int      count;

    pthread_mutex_t mutex;
    sem_t           sem_empty;
    sem_t           sem_full;

    /* ticket-based fair lock */
    atomic_int      ticket_next;
    atomic_int      ticket_now;
    pthread_mutex_t fair_mutex;
    pthread_cond_t  fair_cond;
} Buffer;

/* ── Per-thread statistics ──────────────────────────────────────────────── */
typedef struct {
    int      id;
    char     role[16];
    long     ops;
    double   total_wait_ms;
    int      rate_ms;
} ThreadStats;

/* ── Log queue ──────────────────────────────────────────────────────────── */
typedef struct { char msg[256]; } LogEntry;

typedef struct {
    LogEntry        entries[LOG_QUEUE_SIZE];
    int             head, tail, count;
    pthread_mutex_t mutex;
    sem_t           sem_items;
    sem_t           sem_space;

    /* ring of last N entries for GUI panel */
    char            panel[LOG_PANEL_ROWS][320];
    int             panel_next;
    pthread_mutex_t panel_mutex;
} LogQueue;

/* ── Config ─────────────────────────────────────────────────────────────── */
typedef struct {
    int buf_size;
    int num_producers;
    int num_consumers;
    int prod_rate_ms;
    int cons_rate_ms;
    int duration_sec;
    int fair;
    int no_gui;      /* 1 = plain console mode (for tests) */
} Config;

/* ── Globals ────────────────────────────────────────────────────────────── */
extern Buffer      g_buf;
extern LogQueue    g_logq;
extern Config      g_cfg;
extern ThreadStats g_pstats[MAX_THREADS];
extern ThreadStats g_cstats[MAX_THREADS];
extern atomic_int  g_shutdown;
extern atomic_long g_total_produced;
extern atomic_long g_total_consumed;

/* ── Prototypes ─────────────────────────────────────────────────────────── */
/* buffer.c */
void  buffer_init(Buffer *b, int size);
void  buffer_destroy(Buffer *b);
void  buffer_produce(Buffer *b, int item, ThreadStats *st);
int   buffer_consume(Buffer *b, ThreadStats *st);

/* logger.c */
void  logq_init(LogQueue *q);
void  logq_destroy(LogQueue *q);
void  logq_push(LogQueue *q, const char *fmt, ...);
void *logger_thread(void *arg);

/* threads.c */
void *producer_thread(void *arg);
void *consumer_thread(void *arg);

/* gui.c */
void *gui_thread(void *arg);
void  gui_cleanup(void);

/* visualizer.c (plain console fallback) */
void  visualize_plain(Buffer *b);
void  print_stats(void);

/* utils.c */
double now_ms(void);
void   sleep_ms(int ms);
void   parse_args(int argc, char **argv, Config *cfg);
void   print_usage(const char *prog);

#endif
