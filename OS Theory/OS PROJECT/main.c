#include "shared.h"

/* ── Global definitions ─────────────────────────────────────────────────── */
Buffer      g_buf;
LogQueue    g_logq;
Config      g_cfg;
ThreadStats g_pstats[MAX_THREADS];
ThreadStats g_cstats[MAX_THREADS];
atomic_int  g_shutdown       = 0;
atomic_long g_total_produced = 0;
atomic_long g_total_consumed = 0;

/* ── Signal handler ─────────────────────────────────────────────────────── */
static void handle_sigint(int sig) {
    (void)sig;
    atomic_store(&g_shutdown, 1);
}

/* ── Plain console banner (used in no-GUI mode) ─────────────────────────── */
static void print_banner(void) {
    printf("\033[2J\033[H");
    printf("\033[1;36m"
           "╔══════════════════════════════════════════════════════════╗\n"
           "║       Multi-Threaded Producer-Consumer Simulation        ║\n"
           "║         CS-2006 Operating Systems  |  FAST-NUCES         ║\n"
           "╚══════════════════════════════════════════════════════════╝\n"
           "\033[0m\n");
    printf("  Buffer: %d   Producers: %d (%d ms)   Consumers: %d (%d ms)\n",
           g_cfg.buf_size, g_cfg.num_producers, g_cfg.prod_rate_ms,
           g_cfg.num_consumers, g_cfg.cons_rate_ms);
    printf("  Duration: %d s   Fair: %s   Log: %s\n\n",
           g_cfg.duration_sec, g_cfg.fair ? "ON" : "OFF", LOG_FILE);
    printf("  Press Ctrl+C to stop early.\n\n"
           "  Live Buffer State:\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    parse_args(argc, argv, &g_cfg);

    buffer_init(&g_buf, g_cfg.buf_size);
    logq_init(&g_logq);
    signal(SIGINT, handle_sigint);

    /* start logger */
    pthread_t log_tid;
    pthread_create(&log_tid, NULL, logger_thread, NULL);

    /* start GUI or plain banner */
    pthread_t gui_tid;
    if (!g_cfg.no_gui) {
        pthread_create(&gui_tid, NULL, gui_thread, NULL);
    } else {
        print_banner();
    }

    /* start producers */
    pthread_t prod_tids[MAX_THREADS];
    for (int i = 0; i < g_cfg.num_producers; i++) {
        g_pstats[i] = (ThreadStats){
            .id = i + 1, .ops = 0, .total_wait_ms = 0.0,
            .rate_ms = g_cfg.prod_rate_ms
        };
        snprintf(g_pstats[i].role, sizeof(g_pstats[i].role), "Producer-%d", i+1);
        pthread_create(&prod_tids[i], NULL, producer_thread, &g_pstats[i]);
    }

    /* start consumers */
    pthread_t cons_tids[MAX_THREADS];
    for (int i = 0; i < g_cfg.num_consumers; i++) {
        g_cstats[i] = (ThreadStats){
            .id = i + 1, .ops = 0, .total_wait_ms = 0.0,
            .rate_ms = g_cfg.cons_rate_ms
        };
        snprintf(g_cstats[i].role, sizeof(g_cstats[i].role), "Consumer-%d", i+1);
        pthread_create(&cons_tids[i], NULL, consumer_thread, &g_cstats[i]);
    }

    logq_push(&g_logq, "[INFO ] Simulation started  buf=%d  P=%d  C=%d  fair=%s",
              g_cfg.buf_size, g_cfg.num_producers, g_cfg.num_consumers,
              g_cfg.fair ? "on" : "off");

    /* main loop: plain visualizer + duration timer */
    double start = now_ms();
    while (!atomic_load(&g_shutdown)) {
        if (g_cfg.no_gui)
            visualize_plain(&g_buf);
        sleep_ms(250);
        if (g_cfg.duration_sec > 0) {
            double elapsed = (now_ms() - start) / 1000.0;
            if (elapsed >= g_cfg.duration_sec)
                atomic_store(&g_shutdown, 1);
        }
    }

    logq_push(&g_logq, "[INFO ] Shutdown initiated — draining buffer");

    /* join producers */
    for (int i = 0; i < g_cfg.num_producers; i++)
        pthread_join(prod_tids[i], NULL);

    /* join consumers (they drain remaining items first) */
    for (int i = 0; i < g_cfg.num_consumers; i++)
        pthread_join(cons_tids[i], NULL);

    logq_push(&g_logq, "[INFO ] All threads joined. Produced=%ld Consumed=%ld",
              (long)atomic_load(&g_total_produced),
              (long)atomic_load(&g_total_consumed));

    /* join GUI thread */
    if (!g_cfg.no_gui)
        pthread_join(gui_tid, NULL);

    /* join logger */
    pthread_join(log_tid, NULL);

    /* final stats to console */
    if (g_cfg.no_gui) {
        printf("\n\n");
    }
    print_stats();
    printf("  Log saved to: \033[1m%s\033[0m\n\n", LOG_FILE);

    buffer_destroy(&g_buf);
    logq_destroy(&g_logq);
    return 0;
}
