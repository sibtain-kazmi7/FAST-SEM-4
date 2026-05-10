#include "shared.h"

void visualize_plain(Buffer *b) {
    pthread_mutex_lock(&b->mutex);
    int count = b->count;
    int size  = b->size;
    pthread_mutex_unlock(&b->mutex);

    int bar_w  = 50;
    int filled = (size > 0) ? (count * bar_w / size) : 0;
    double pct = (size > 0) ? (100.0 * count / size) : 0.0;
    if (size == 0) size = g_cfg.buf_size;  /* fallback before buffer fully init */

    const char *col = (pct < 60.0) ? "\033[1;32m" :
                      (pct < 85.0) ? "\033[1;33m" : "\033[1;31m";

    printf("\r  Buffer [");
    for (int i = 0; i < bar_w; i++)
        printf("%s%s\033[0m", col, i < filled ? "#" : ".");
    printf("] %s%3d\033[0m/%d (%5.1f%%)  "
           "P:%ld C:%ld   ",
           col, count, size, pct,
           (long)atomic_load(&g_total_produced),
           (long)atomic_load(&g_total_consumed));
    fflush(stdout);
}

void print_stats(void) {
    printf("\n\n\033[1;36m"
           "╔══════════════════════════════════════════════════════╗\n"
           "║              THREAD STATISTICS SUMMARY              ║\n"
           "╚══════════════════════════════════════════════════════╝\n"
           "\033[0m");

    printf("\033[1m  %-14s  %6s  %12s  %10s\n\033[0m",
           "Thread", "Ops", "AvgWait(ms)", "Rate(ms)");
    printf("  %-14s  %6s  %12s  %10s\n",
           "--------------","------","------------","--------");

    long tp_ops = 0, tc_ops = 0;
    double tp_w = 0.0, tc_w = 0.0;

    for (int i = 0; i < g_cfg.num_producers; i++) {
        ThreadStats *s = &g_pstats[i];
        double avg = (s->ops > 0) ? s->total_wait_ms / s->ops : 0.0;
        printf("  \033[1;32m%-14s\033[0m  %6ld  %12.3f  %10d\n",
               s->role, s->ops, avg, s->rate_ms);
        tp_ops += s->ops; tp_w += s->total_wait_ms;
    }
    for (int i = 0; i < g_cfg.num_consumers; i++) {
        ThreadStats *s = &g_cstats[i];
        double avg = (s->ops > 0) ? s->total_wait_ms / s->ops : 0.0;
        printf("  \033[1;36m%-14s\033[0m  %6ld  %12.3f  %10d\n",
               s->role, s->ops, avg, s->rate_ms);
        tc_ops += s->ops; tc_w += s->total_wait_ms;
    }

    printf("\n\033[1m  %-24s %ld\n\033[0m", "Total Produced:", atomic_load(&g_total_produced));
    printf("\033[1m  %-24s %ld\n\033[0m",   "Total Consumed:", atomic_load(&g_total_consumed));
    printf("\033[1m  %-24s %.3f ms\n\033[0m","Avg Producer Wait:",
           tp_ops > 0 ? tp_w / tp_ops : 0.0);
    printf("\033[1m  %-24s %.3f ms\n\n\033[0m","Avg Consumer Wait:",
           tc_ops > 0 ? tc_w / tc_ops : 0.0);

    /* fairness spread */
    if (g_cfg.num_producers > 1) {
        long mx = 0, mn = LONG_MAX;
        for (int i = 0; i < g_cfg.num_producers; i++) {
            if (g_pstats[i].ops > mx) mx = g_pstats[i].ops;
            if (g_pstats[i].ops < mn) mn = g_pstats[i].ops;
        }
        printf("  Producer fairness spread: %ld ops\n", mx - mn);
    }
    if (g_cfg.num_consumers > 1) {
        long mx = 0, mn = LONG_MAX;
        for (int i = 0; i < g_cfg.num_consumers; i++) {
            if (g_cstats[i].ops > mx) mx = g_cstats[i].ops;
            if (g_cstats[i].ops < mn) mn = g_cstats[i].ops;
        }
        printf("  Consumer fairness spread: %ld ops\n\n", mx - mn);
    }
}
