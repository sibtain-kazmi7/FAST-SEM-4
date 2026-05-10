#include "shared.h"

static WINDOW *win_header  = NULL;
static WINDOW *win_buf     = NULL;
static WINDOW *win_threads = NULL;
static WINDOW *win_log     = NULL;
static WINDOW *win_footer  = NULL;

/* ── Safe ncurses colour pairs ──────────────────────────────────────────── */
#define CP_NORMAL   1
#define CP_GREEN    2
#define CP_YELLOW   3
#define CP_RED      4
#define CP_CYAN     5
#define CP_HEADER   6
#define CP_BORDER   7
#define CP_PRODUCER 8
#define CP_CONSUMER 9

static void init_colours(void) {
    start_color();
    use_default_colors();
    init_pair(CP_NORMAL,   COLOR_WHITE,   -1);
    init_pair(CP_GREEN,    COLOR_GREEN,   -1);
    init_pair(CP_YELLOW,   COLOR_YELLOW,  -1);
    init_pair(CP_RED,      COLOR_RED,     -1);
    init_pair(CP_CYAN,     COLOR_CYAN,    -1);
    init_pair(CP_HEADER,   COLOR_BLACK,   COLOR_WHITE);
    init_pair(CP_BORDER,   COLOR_WHITE,   -1);
    init_pair(CP_PRODUCER, COLOR_GREEN,   -1);
    init_pair(CP_CONSUMER, COLOR_CYAN,    -1);
}

/* ── Draw a titled bordered window ─────────────────────────────────────── */
static void draw_box(WINDOW *w, const char *title) {
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));
    if (title) {
        wattron(w, A_BOLD | COLOR_PAIR(CP_NORMAL));
        mvwprintw(w, 0, 2, " %s ", title);
        wattroff(w, A_BOLD | COLOR_PAIR(CP_NORMAL));
    }
}

/* ── Header panel ───────────────────────────────────────────────────────── */
static void draw_header(void) {
    werase(win_header);
    wattron(win_header, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwprintw(win_header, 0, 0,
        "  Multi-Threaded Producer-Consumer  |  CS-2006 OS  |  FAST-NUCES  |  Spring 2026  ");
    wattroff(win_header, COLOR_PAIR(CP_HEADER) | A_BOLD);
    wnoutrefresh(win_header);
}

/* ── Buffer bar panel ───────────────────────────────────────────────────── */
static void draw_buffer(void) {
    werase(win_buf);
    draw_box(win_buf, "Buffer State");

    pthread_mutex_lock(&g_buf.mutex);
    int count = g_buf.count;
    int size  = g_buf.size;
    pthread_mutex_unlock(&g_buf.mutex);

    double pct    = (size > 0) ? (100.0 * count / size) : 0.0;
    int    bar_w  = getmaxx(win_buf) - 20;
    if (bar_w < 10) bar_w = 10;
    int    filled = (size > 0) ? (int)(count * bar_w / size) : 0;

    int cp = (pct < 60.0) ? CP_GREEN : (pct < 85.0) ? CP_YELLOW : CP_RED;

    mvwprintw(win_buf, 1, 2, "Occupancy: %3d / %3d  (%5.1f%%)", count, size, pct);

    mvwaddstr(win_buf, 2, 2, "[");
    wattron(win_buf, COLOR_PAIR(cp) | A_BOLD);
    for (int i = 0; i < bar_w; i++)
        waddch(win_buf, i < filled ? '#' : '.');
    wattroff(win_buf, COLOR_PAIR(cp) | A_BOLD);
    waddstr(win_buf, "]");

    long prod = atomic_load(&g_total_produced);
    long cons = atomic_load(&g_total_consumed);
    mvwprintw(win_buf, 3, 2,
        "Total Produced: %-8ld   Total Consumed: %-8ld", prod, cons);

    mvwprintw(win_buf, 4, 2,
        "Producers: %d   Consumers: %d   Buffer: %d   Fair: %s",
        g_cfg.num_producers, g_cfg.num_consumers, g_cfg.buf_size,
        g_cfg.fair ? "ON" : "OFF");

    wnoutrefresh(win_buf);
}

/* ── Thread status panel ────────────────────────────────────────────────── */
static void draw_threads(void) {
    werase(win_threads);
    draw_box(win_threads, "Thread Activity");

    int row = 1;
    int max_rows = getmaxy(win_threads) - 2;

    /* header row */
    wattron(win_threads, A_BOLD | COLOR_PAIR(CP_NORMAL));
    mvwprintw(win_threads, row++, 2,
        "%-14s  %6s  %12s  %8s",
        "Thread", "Ops", "AvgWait(ms)", "Rate(ms)");
    wattroff(win_threads, A_BOLD | COLOR_PAIR(CP_NORMAL));

    for (int i = 0; i < g_cfg.num_producers && row < max_rows; i++) {
        ThreadStats *s = &g_pstats[i];
        double avg = (s->ops > 0) ? s->total_wait_ms / s->ops : 0.0;
        wattron(win_threads, COLOR_PAIR(CP_PRODUCER));
        mvwprintw(win_threads, row++, 2,
            "%-14s  %6ld  %12.3f  %8d",
            s->role, s->ops, avg, s->rate_ms);
        wattroff(win_threads, COLOR_PAIR(CP_PRODUCER));
    }

    for (int i = 0; i < g_cfg.num_consumers && row < max_rows; i++) {
        ThreadStats *s = &g_cstats[i];
        double avg = (s->ops > 0) ? s->total_wait_ms / s->ops : 0.0;
        wattron(win_threads, COLOR_PAIR(CP_CONSUMER));
        mvwprintw(win_threads, row++, 2,
            "%-14s  %6ld  %12.3f  %8d",
            s->role, s->ops, avg, s->rate_ms);
        wattroff(win_threads, COLOR_PAIR(CP_CONSUMER));
    }

    wnoutrefresh(win_threads);
}

/* ── Log panel ──────────────────────────────────────────────────────────── */
static void draw_log(void) {
    werase(win_log);
    draw_box(win_log, "Live Log  (last 10 events)");

    int max_w = getmaxx(win_log) - 4;
    pthread_mutex_lock(&g_logq.panel_mutex);
    for (int i = 0; i < LOG_PANEL_ROWS; i++) {
        /* iterate in order from oldest to newest */
        int idx = (g_logq.panel_next + i) % LOG_PANEL_ROWS;
        char buf[256];
        strncpy(buf, g_logq.panel[idx], sizeof(buf) - 1);
        buf[sizeof(buf)-1] = '\0';
        if (buf[0] == '\0') continue;

        /* colour-code by type */
        int cp = CP_NORMAL;
        if      (strstr(buf, "[PRODUCE]")) cp = CP_GREEN;
        else if (strstr(buf, "[CONSUME]")) cp = CP_CYAN;
        else if (strstr(buf, "[INFO ]"))   cp = CP_YELLOW;

        /* truncate to window width */
        if ((int)strlen(buf) > max_w) buf[max_w] = '\0';

        wattron(win_log, COLOR_PAIR(cp));
        mvwprintw(win_log, 1 + i, 2, "%s", buf);
        wattroff(win_log, COLOR_PAIR(cp));
    }
    pthread_mutex_unlock(&g_logq.panel_mutex);

    wnoutrefresh(win_log);
}

/* ── Footer panel ───────────────────────────────────────────────────────── */
static void draw_footer(void) {
    werase(win_footer);
    wattron(win_footer, COLOR_PAIR(CP_HEADER));
    mvwprintw(win_footer, 0, 0,
        "  [q / Ctrl+C] Quit   |   Green=Producer   Cyan=Consumer   |   Log: %s  ",
        LOG_FILE);
    wattroff(win_footer, COLOR_PAIR(CP_HEADER));
    wnoutrefresh(win_footer);
}

/* ── Layout calculation ─────────────────────────────────────────────────── */
static void create_windows(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* minimum terminal size guard */
    if (rows < 24 || cols < 80) {
        endwin();
        fprintf(stderr, "Terminal too small: need at least 80x24, got %dx%d\n", cols, rows);
        exit(1);
    }

    int buf_h     = 6;
    int footer_h  = 1;
    int header_h  = 1;
    int remain    = rows - buf_h - footer_h - header_h;
    int thread_h  = remain / 2 + remain % 2;
    int log_h     = remain / 2;

    int y = 0;
    win_header  = newwin(header_h,  cols, y, 0); y += header_h;
    win_buf     = newwin(buf_h,     cols, y, 0); y += buf_h;
    win_threads = newwin(thread_h,  cols, y, 0); y += thread_h;
    win_log     = newwin(log_h,     cols, y, 0); y += log_h;
    win_footer  = newwin(footer_h,  cols, y, 0);
}

/* ── GUI cleanup ────────────────────────────────────────────────────────── */
void gui_cleanup(void) {
    if (win_header)  { delwin(win_header);  win_header  = NULL; }
    if (win_buf)     { delwin(win_buf);     win_buf     = NULL; }
    if (win_threads) { delwin(win_threads); win_threads = NULL; }
    if (win_log)     { delwin(win_log);     win_log     = NULL; }
    if (win_footer)  { delwin(win_footer);  win_footer  = NULL; }
    endwin();
}

/* ── GUI thread entry point ─────────────────────────────────────────────── */
void *gui_thread(void *arg) {
    (void)arg;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);   /* non-blocking getch */
    init_colours();
    create_windows();

    while (!atomic_load(&g_shutdown)) {
        draw_header();
        draw_buffer();
        draw_threads();
        draw_log();
        draw_footer();
        doupdate();

        /* check for 'q' key */
        int ch = getch();
        if (ch == 'q' || ch == 'Q')
            atomic_store(&g_shutdown, 1);

        sleep_ms(250);
    }

    /* final refresh before exiting */
    draw_header();
    draw_buffer();
    draw_threads();
    draw_log();
    draw_footer();
    doupdate();

    sleep_ms(500);
    gui_cleanup();
    return NULL;
}
