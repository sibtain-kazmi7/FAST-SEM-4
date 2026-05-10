#include "shared.h"

double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

void print_usage(const char *prog) {
    printf(
        "\nUsage: %s [OPTIONS]\n\n"
        "Options:\n"
        "  -b <size>   Buffer size          (default: 10,  max: %d)\n"
        "  -p <n>      Number of producers  (default: 2,   max: %d)\n"
        "  -c <n>      Number of consumers  (default: 2,   max: %d)\n"
        "  -P <ms>     Producer sleep (ms)  (default: 300)\n"
        "  -C <ms>     Consumer sleep (ms)  (default: 500)\n"
        "  -d <sec>    Duration in seconds  (default: 15)\n"
        "  -f          Enable fair scheduling (ticket-based)\n"
        "  -n          No GUI  (plain console mode)\n"
        "  -h          Show this help\n\n"
        "Examples:\n"
        "  %s -b 10 -p 3 -c 2 -P 200 -C 400 -d 20 -f\n"
        "  %s -b 1  -p 1 -c 1 -P 100 -C 100 -d 5  -n\n\n",
        prog, MAX_BUFFER, MAX_THREADS, MAX_THREADS, prog, prog
    );
}

void parse_args(int argc, char **argv, Config *cfg) {
    cfg->buf_size      = 10;
    cfg->num_producers = 2;
    cfg->num_consumers = 2;
    cfg->prod_rate_ms  = 300;
    cfg->cons_rate_ms  = 500;
    cfg->duration_sec  = 15;
    cfg->fair          = 0;
    cfg->no_gui        = 0;

    int opt;
    while ((opt = getopt(argc, argv, "b:p:c:P:C:d:fnh")) != -1) {
        switch (opt) {
            case 'b': cfg->buf_size      = atoi(optarg); break;
            case 'p': cfg->num_producers = atoi(optarg); break;
            case 'c': cfg->num_consumers = atoi(optarg); break;
            case 'P': cfg->prod_rate_ms  = atoi(optarg); break;
            case 'C': cfg->cons_rate_ms  = atoi(optarg); break;
            case 'd': cfg->duration_sec  = atoi(optarg); break;
            case 'f': cfg->fair          = 1;            break;
            case 'n': cfg->no_gui        = 1;            break;
            case 'h': print_usage(argv[0]); exit(0);
            default:  print_usage(argv[0]); exit(1);
        }
    }

    if (cfg->buf_size < 1 || cfg->buf_size > MAX_BUFFER) {
        fprintf(stderr, "Error: Buffer size must be 1-%d\n", MAX_BUFFER); exit(1);
    }
    if (cfg->num_producers < 1 || cfg->num_producers > MAX_THREADS) {
        fprintf(stderr, "Error: Producers must be 1-%d\n", MAX_THREADS); exit(1);
    }
    if (cfg->num_consumers < 1 || cfg->num_consumers > MAX_THREADS) {
        fprintf(stderr, "Error: Consumers must be 1-%d\n", MAX_THREADS); exit(1);
    }
    if (cfg->prod_rate_ms < 1 || cfg->cons_rate_ms < 1) {
        fprintf(stderr, "Error: Rate must be >= 1 ms\n"); exit(1);
    }
}
