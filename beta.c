 * RING BUFFER KERNEL LOG
 * ========================================================= */
/* (appended module — extends minios.c past 1600 lines)     */

#define KLOG_ENTRIES   128
#define KLOG_MSG_LEN   128

typedef enum {
    KLOG_DEBUG = 0,
    KLOG_INFO,
    KLOG_WARN,
    KLOG_ERROR
} KLogLevel;

typedef struct {
    KLogLevel level;
    time_t    ts;
    char      msg[KLOG_MSG_LEN];
} KLogEntry;

static KLogEntry klog_buf[KLOG_ENTRIES];
static int       klog_head  = 0;
static int       klog_count = 0;

static void klog_write(KLogLevel lvl, const char *fmt, ...) {
    KLogEntry *e = &klog_buf[klog_head % KLOG_ENTRIES];
    e->level = lvl;
    e->ts    = time(NULL);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, KLOG_MSG_LEN - 1, fmt, ap);
    va_end(ap);
    klog_head++;
    if (klog_count < KLOG_ENTRIES) klog_count++;
}

static const char *klog_level_str(KLogLevel l) {
    switch (l) {
        case KLOG_DEBUG: return "DEBUG";
        case KLOG_INFO:  return "INFO ";
        case KLOG_WARN:  return "WARN ";
        case KLOG_ERROR: return "ERROR";
        default:         return "?    ";
    }
}

static void klog_dump(int n) {
    int total   = klog_count < KLOG_ENTRIES ? klog_count : KLOG_ENTRIES;
    int start   = (n > 0 && n < total) ? total - n : 0;
    int base    = (klog_count > KLOG_ENTRIES) ? klog_head : 0;

    printf(CLR_CYAN "\n=== Kernel Log (last %d entries) ===\n" CLR_RESET, total - start);
    for (int i = start; i < total; i++) {
        KLogEntry *e = &klog_buf[(base + i) % KLOG_ENTRIES];
        char tbuf[20];
        struct tm *tm_info = localtime(&e->ts);
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);
        const char *color =
            e->level == KLOG_DEBUG ? CLR_RESET :
            e->level == KLOG_INFO  ? CLR_GREEN :
            e->level == KLOG_WARN  ? CLR_YELLOW : CLR_RED;
        printf("  [%s] %s%s%s  %s\n", tbuf, color, klog_level_str(e->level), CLR_RESET, e->msg);
    }
    printf("====================================\n\n");
}

/* =========================================================
 * CRON-STYLE TASK SCHEDULER
 * ========================================================= */

#define MAX_CRON_JOBS  16
#define CRON_NAME_LEN  32
#define CRON_CMD_LEN   128

typedef struct {
    int  used;
    int  id;
    int  interval_ticks;   /* run every N ticks */
    int  ticks_remaining;
    int  run_count;
    char name[CRON_NAME_LEN];
    char cmd[CRON_CMD_LEN];
} CronJob;

static CronJob cron_jobs[MAX_CRON_JOBS];
static int     cron_next_id = 1;
static int     cron_tick_count = 0;

static void cron_init(void) {
    memset(cron_jobs, 0, sizeof(cron_jobs));
    /* Pre-register a memory snapshot job */
    cron_jobs[0].used             = 1;
    cron_jobs[0].id               = cron_next_id++;
    cron_jobs[0].interval_ticks   = 10;
    cron_jobs[0].ticks_remaining  = 10;
    cron_jobs[0].run_count        = 0;
    strncpy(cron_jobs[0].name, "mem-snapshot", CRON_NAME_LEN-1);
    strncpy(cron_jobs[0].cmd, "meminfo", CRON_CMD_LEN-1);

    klog_write(KLOG_INFO, "cron: initialized with 1 default job");
}

static int cron_add(const char *name, int interval, const char *cmd) {
    for (int i = 0; i < MAX_CRON_JOBS; i++) {
        if (!cron_jobs[i].used) {
            cron_jobs[i].used            = 1;
            cron_jobs[i].id              = cron_next_id++;
            cron_jobs[i].interval_ticks  = interval;
            cron_jobs[i].ticks_remaining = interval;
            cron_jobs[i].run_count       = 0;
            strncpy(cron_jobs[i].name, name, CRON_NAME_LEN-1);
            strncpy(cron_jobs[i].cmd,  cmd,  CRON_CMD_LEN-1);
            klog_write(KLOG_INFO, "cron: added job '%s' every %d ticks", name, interval);
            return cron_jobs[i].id;
        }
    }
    return -1;
}

static int cron_remove(int id) {
    for (int i = 0; i < MAX_CRON_JOBS; i++) {
        if (cron_jobs[i].used && cron_jobs[i].id == id) {
            memset(&cron_jobs[i], 0, sizeof(CronJob));
            klog_write(KLOG_INFO, "cron: removed job id=%d", id);
            return 0;
        }
    }
    return -1;
}

static void cron_tick(void) {
    cron_tick_count++;
    for (int i = 0; i < MAX_CRON_JOBS; i++) {
        if (!cron_jobs[i].used) continue;
        cron_jobs[i].ticks_remaining--;
        if (cron_jobs[i].ticks_remaining <= 0) {
            cron_jobs[i].ticks_remaining = cron_jobs[i].interval_ticks;
            cron_jobs[i].run_count++;
            klog_write(KLOG_DEBUG, "cron: fired job '%s' (run #%d)",
                       cron_jobs[i].name, cron_jobs[i].run_count);
            /* Jobs are simulated; we do NOT run them in a loop to avoid recursion */
        }
    }
}

static void cron_list(void) {
    printf(CLR_CYAN "\n%-4s %-20s %-8s %-8s %s\n" CLR_RESET,
           "ID", "NAME", "INTERVAL", "RUNS", "COMMAND");
    printf("------------------------------------------------------\n");
    for (int i = 0; i < MAX_CRON_JOBS; i++) {
        if (!cron_jobs[i].used) continue;
        printf("%-4d %-20s %-8d %-8d %s\n",
               cron_jobs[i].id,
               cron_jobs[i].name,
               cron_jobs[i].interval_ticks,
               cron_jobs[i].run_count,
               cron_jobs[i].cmd);
    }
    printf("\n");
}

/* =========================================================
 * NETWORK INTERFACE SIMULATION
 * ========================================================= */

#define MAX_NET_IFACES  4
#define IFACE_NAME_LEN  16
#define IP_LEN          16

typedef struct {
    int    used;
    char   name[IFACE_NAME_LEN];
    char   ip[IP_LEN];
    char   mask[IP_LEN];
    char   mac[18];
    int    up;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_pkts;
    uint64_t tx_pkts;
} NetIface;

static NetIface net_ifaces[MAX_NET_IFACES];

static void net_init(void) {
    memset(net_ifaces, 0, sizeof(net_ifaces));

    strncpy(net_ifaces[0].name, "lo",          IFACE_NAME_LEN-1);
    strncpy(net_ifaces[0].ip,   "127.0.0.1",   IP_LEN-1);
    strncpy(net_ifaces[0].mask, "255.0.0.0",   IP_LEN-1);
    snprintf(net_ifaces[0].mac, 18, "00:00:00:00:00:00");
    net_ifaces[0].used = 1;
    net_ifaces[0].up   = 1;
    net_ifaces[0].rx_bytes = 1024;
    net_ifaces[0].tx_bytes = 1024;

    strncpy(net_ifaces[1].name, "eth0",         IFACE_NAME_LEN-1);
    strncpy(net_ifaces[1].ip,   "192.168.1.10", IP_LEN-1);
    strncpy(net_ifaces[1].mask, "255.255.255.0",IP_LEN-1);
    snprintf(net_ifaces[1].mac, 18, "AA:BB:CC:DD:EE:FF");
    net_ifaces[1].used = 1;
    net_ifaces[1].up   = 1;
    net_ifaces[1].rx_bytes = 1048576;
    net_ifaces[1].tx_bytes = 204800;
    net_ifaces[1].rx_pkts  = 8192;
    net_ifaces[1].tx_pkts  = 2048;

    klog_write(KLOG_INFO, "net: initialized lo and eth0");
}

static void net_ifconfig(void) {
    for (int i = 0; i < MAX_NET_IFACES; i++) {
        if (!net_ifaces[i].used) continue;
        printf(CLR_BOLD "%s" CLR_RESET ": flags=%s  mtu 1500\n",
               net_ifaces[i].name,
               net_ifaces[i].up ? "UP,LOOPBACK,RUNNING" : "DOWN");
        printf("        inet %-15s  netmask %s\n",
               net_ifaces[i].ip, net_ifaces[i].mask);
        printf("        ether %s\n", net_ifaces[i].mac);
        printf("        RX bytes: %-12llu  pkts: %llu\n",
               (unsigned long long)net_ifaces[i].rx_bytes,
               (unsigned long long)net_ifaces[i].rx_pkts);
        printf("        TX bytes: %-12llu  pkts: %llu\n\n",
               (unsigned long long)net_ifaces[i].tx_bytes,
               (unsigned long long)net_ifaces[i].tx_pkts);
    }
}

static int net_set_ip(const char *iface, const char *ip) {
    for (int i = 0; i < MAX_NET_IFACES; i++) {
        if (net_ifaces[i].used && strcmp(net_ifaces[i].name, iface) == 0) {
            strncpy(net_ifaces[i].ip, ip, IP_LEN-1);
            klog_write(KLOG_INFO, "net: %s IP changed to %s", iface, ip);
            return 0;
        }
    }
    return -1;
}

static void net_simulate_traffic(void) {
    /* Bump counters to simulate traffic */
    for (int i = 0; i < MAX_NET_IFACES; i++) {
        if (!net_ifaces[i].used || !net_ifaces[i].up) continue;
        net_ifaces[i].rx_bytes += (uint64_t)(rand() % 4096);
        net_ifaces[i].tx_bytes += (uint64_t)(rand() % 2048);
        net_ifaces[i].rx_pkts  += (uint64_t)(rand() % 8);
        net_ifaces[i].tx_pkts  += (uint64_t)(rand() % 4);
    }
}

/* KEY = X */

#define MAX_SYSCALLS  32

typedef long (*syscall_fn)(long, long, long);

typedef struct {
    int         num;
    const char *name;
    syscall_fn  fn;
    uint64_t    call_count;
} SyscallEntry;

static long sys_write_impl(long fd, long buf, long count) {
    (void)fd;
    if (buf && count > 0) {
        fwrite((void *)buf, 1, (size_t)count, stdout);
        return count;
    }
    return -1;
}

static long sys_getpid_impl(long a, long b, long c) {
    (void)a; (void)b; (void)c;
    return current_pid;
}

static long sys_exit_impl(long code, long b, long c) {
    (void)b; (void)c;
    klog_write(KLOG_INFO, "sys_exit called with code %ld", code);
    return 0;
}

static long sys_time_impl(long a, long b, long c) {
    (void)a; (void)b; (void)c;
    return (long)time(NULL);
}

static long sys_malloc_impl(long size, long b, long c) {
    (void)b; (void)c;
    void *p = mm_malloc((size_t)size);
    return (long)p;
}

static SyscallEntry syscall_table[MAX_SYSCALLS] = {
    {  1, "write",   sys_write_impl,  0 },
    {  2, "getpid",  sys_getpid_impl, 0 },
    {  3, "exit",    sys_exit_impl,   0 },
    {  4, "time",    sys_time_impl,   0 },
    {  5, "malloc",  sys_malloc_impl, 0 },
    {  0, NULL,      NULL,            0 }
};

static long do_syscall(int num, long a, long b, long c) {
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        if (syscall_table[i].name == NULL) break;
        if (syscall_table[i].num == num) {
            syscall_table[i].call_count++;
            klog_write(KLOG_DEBUG, "syscall %d (%s) invoked", num, syscall_table[i].name);
            return syscall_table[i].fn(a, b, c);
        }
    }
    klog_write(KLOG_WARN, "unknown syscall %d", num);
    return -ENOSYS;
}

static void syscall_stats(void) {
    printf(CLR_CYAN "\n=== Syscall Statistics ===\n" CLR_RESET);
    printf("  %-6s %-16s %s\n", "NUM", "NAME", "CALLS");
    printf("  ----------------------------------\n");
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        if (!syscall_table[i].name) break;
        printf("  %-6d %-16s %llu\n",
               syscall_table[i].num,
               syscall_table[i].name,
               (unsigned long long)syscall_table[i].call_count);
    }
    printf("\n");
}

/* =========================================================
 * PIPE BUFFER (IPC SIMULATION)
 * ========================================================= */

#define MAX_PIPES   8
#define PIPE_BUF_SZ 1024

typedef struct {
    int    used;
    int    id;
    char   buf[PIPE_BUF_SZ];
    int    read_pos;
    int    write_pos;
    int    len;
} Pipe;

static Pipe pipes[MAX_PIPES];
static int  pipe_next_id = 1;

static void pipe_init(void) {
    memset(pipes, 0, sizeof(pipes));
}

static int pipe_open(void) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].used) {
            pipes[i].used     = 1;
            pipes[i].id       = pipe_next_id++;
            pipes[i].read_pos = 0;
            pipes[i].write_pos= 0;
            pipes[i].len      = 0;
            return pipes[i].id;
        }
    }
    return -1;
}

static int pipe_write(int id, const char *data, int len) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].used && pipes[i].id == id) {
            int avail = PIPE_BUF_SZ - pipes[i].len;
            if (len > avail) len = avail;
            for (int j = 0; j < len; j++) {
                pipes[i].buf[pipes[i].write_pos] = data[j];
                pipes[i].write_pos = (pipes[i].write_pos + 1) % PIPE_BUF_SZ;
            }
            pipes[i].len += len;
            return len;
        }
    }
    return -1;
}

static int pipe_read(int id, char *out, int maxlen) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].used && pipes[i].id == id) {
            int n = pipes[i].len < maxlen ? pipes[i].len : maxlen;
            for (int j = 0; j < n; j++) {
                out[j] = pipes[i].buf[pipes[i].read_pos];
                pipes[i].read_pos = (pipes[i].read_pos + 1) % PIPE_BUF_SZ;
            }
            pipes[i].len -= n;
            return n;
        }
    }
    return -1;
}

static void pipe_close(int id) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].used && pipes[i].id == id) {
            memset(&pipes[i], 0, sizeof(Pipe));
            return;
        }
    }
}

static void pipe_status(void) {
    printf(CLR_CYAN "\n=== Open Pipes ===\n" CLR_RESET);
    int any = 0;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].used) continue;
        printf("  pipe[%d]: %d bytes buffered\n", pipes[i].id, pipes[i].len);
        any = 1;
    }
    if (!any) printf("  (no open pipes)\n");
    printf("\n");
}

/* =========================================================
#include "gama.c"
