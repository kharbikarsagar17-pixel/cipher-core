/*
 * MiniOS - A Mini Operating System Simulator in C
 * Features:
 *   - Interactive shell with built-in commands
 *   - Custom heap memory manager (malloc/free/realloc)
 *   - Process manager with scheduling simulation
 *   - Virtual file system (in-memory)
 *   - Signal handling and system call simulation
 *   - Command history and line editing
 *   - Environment variable store
 *   - Pipe and redirection simulation
 *
 * Build: gcc -o minios minios.c
 * Run:   ./minios
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdarg.h>

/* =========================================================
 * CONSTANTS & CONFIGURATION
 * ========================================================= */

#define OS_NAME             "MiniOS"
#define OS_VERSION          "1.0.0"
#define SHELL_PROMPT        "minios> "

#define HEAP_SIZE           (256 * 1024)   /* 256 KB virtual heap */
#define MAX_PROCESSES       32
#define MAX_FILES           64
#define MAX_FILENAME        64
#define MAX_FILE_SIZE       4096
#define MAX_HISTORY         50
#define MAX_CMD_LEN         1024
#define MAX_ARGS            64
#define MAX_ENV_VARS        64
#define MAX_ENV_KEY         32
#define MAX_ENV_VAL         128
#define MAX_PATH            256

#define BLOCK_MAGIC_FREE    0xDEADBEEF
#define BLOCK_MAGIC_USED    0xCAFEBABE

/* Process states */
#define PROC_UNUSED         0
#define PROC_READY          1
#define PROC_RUNNING        2
#define PROC_SLEEPING       3
#define PROC_ZOMBIE         4

/* File permissions */
#define PERM_READ           0x4
#define PERM_WRITE          0x2
#define PERM_EXEC           0x1

/* Colors */
#define CLR_RESET           "\033[0m"
#define CLR_RED             "\033[31m"
#define CLR_GREEN           "\033[32m"
#define CLR_YELLOW          "\033[33m"
#define CLR_BLUE            "\033[34m"
#define CLR_MAGENTA         "\033[35m"
#define CLR_CYAN            "\033[36m"
#define CLR_BOLD            "\033[1m"

/* =========================================================
 * MEMORY MANAGER
 * ========================================================= */

typedef struct BlockHeader {
    uint32_t        magic;
    size_t          size;       /* payload size (excl. header) */
    struct BlockHeader *next;
    struct BlockHeader *prev;
    int             free;
} BlockHeader;

static uint8_t  heap_pool[HEAP_SIZE];
static BlockHeader *heap_head = NULL;
static size_t   heap_used     = 0;
static size_t   heap_allocs   = 0;
static size_t   heap_frees    = 0;

static void mm_init(void) {
    heap_head        = (BlockHeader *)heap_pool;
    heap_head->magic = BLOCK_MAGIC_FREE;
    heap_head->size  = HEAP_SIZE - sizeof(BlockHeader);
    heap_head->next  = NULL;
    heap_head->prev  = NULL;
    heap_head->free  = 1;
}

static BlockHeader *mm_find_free(size_t size) {
    BlockHeader *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

static void mm_split(BlockHeader *block, size_t size) {
    if (block->size >= size + sizeof(BlockHeader) + 8) {
        BlockHeader *newb = (BlockHeader *)((uint8_t *)block + sizeof(BlockHeader) + size);
        newb->magic = BLOCK_MAGIC_FREE;
        newb->size  = block->size - size - sizeof(BlockHeader);
        newb->free  = 1;
        newb->next  = block->next;
        newb->prev  = block;
        if (block->next) block->next->prev = newb;
        block->next = newb;
        block->size = size;
    }
}

void *mm_malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7) & ~(size_t)7; /* 8-byte align */
    BlockHeader *block = mm_find_free(size);
    if (!block) return NULL;
    mm_split(block, size);
    block->free  = 0;
    block->magic = BLOCK_MAGIC_USED;
    heap_used   += block->size;
    heap_allocs++;
    return (uint8_t *)block + sizeof(BlockHeader);
}

void mm_free(void *ptr) {
    if (!ptr) return;
    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - sizeof(BlockHeader));
    if (block->magic != BLOCK_MAGIC_USED) {
        fprintf(stderr, CLR_RED "[MM] Double-free or corruption detected!\n" CLR_RESET);
        return;
    }
    block->free  = 1;
    block->magic = BLOCK_MAGIC_FREE;
    heap_used   -= block->size;
    heap_frees++;

    /* Coalesce with next */
    if (block->next && block->next->free) {
        block->size += sizeof(BlockHeader) + block->next->size;
        block->next  = block->next->next;
        if (block->next) block->next->prev = block;
    }
    /* Coalesce with prev */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(BlockHeader) + block->size;
        block->prev->next  = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

void *mm_realloc(void *ptr, size_t size) {
    if (!ptr) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }
    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - sizeof(BlockHeader));
    if (block->size >= size) return ptr;
    void *newptr = mm_malloc(size);
    if (!newptr) return NULL;
    memcpy(newptr, ptr, block->size);
    mm_free(ptr);
    return newptr;
}

void *mm_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = mm_malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void mm_dump(void) {
    printf(CLR_CYAN "\n=== Memory Map ===\n" CLR_RESET);
    printf("  Heap size  : %u bytes\n", HEAP_SIZE);
    printf("  In use     : %zu bytes\n", heap_used);
    printf("  Free       : %zu bytes\n", HEAP_SIZE - heap_used);
    printf("  Allocs     : %zu\n", heap_allocs);
    printf("  Frees      : %zu\n", heap_frees);

    BlockHeader *cur = heap_head;
    int idx = 0;
    while (cur) {
        printf("  [Block %3d] addr=%-10p size=%-8zu %s\n",
               idx++,
               (void *)((uint8_t *)cur + sizeof(BlockHeader)),
               cur->size,
               cur->free ? CLR_GREEN "FREE" CLR_RESET : CLR_RED "USED" CLR_RESET);
        cur = cur->next;
    }
    printf("==================\n\n");
}

/* =========================================================
 * PROCESS MANAGER
 * ========================================================= */

typedef void (*proc_func_t)(void *arg);

typedef struct {
    int         pid;
    int         ppid;
    int         state;
    int         priority;
    int         exit_code;
    char        name[32];
    uint64_t    cpu_time;    /* simulated ticks */
    time_t      start_time;
    proc_func_t func;
    void       *arg;
} Process;

static Process  proc_table[MAX_PROCESSES];
static int      next_pid   = 1;
static int      current_pid = 0;

static void pm_init(void) {
    memset(proc_table, 0, sizeof(proc_table));
    /* Create idle process */
    proc_table[0].pid        = 0;
    proc_table[0].ppid       = 0;
    proc_table[0].state      = PROC_RUNNING;
    proc_table[0].priority   = 0;
    proc_table[0].start_time = time(NULL);
    strncpy(proc_table[0].name, "idle", 31);
    /* Create init process */
    proc_table[1].pid        = 1;
    proc_table[1].ppid       = 0;
    proc_table[1].state      = PROC_RUNNING;
    proc_table[1].priority   = 1;
    proc_table[1].start_time = time(NULL);
    strncpy(proc_table[1].name, "init", 31);
    next_pid = 2;
    current_pid = 1;
}

static int pm_spawn(const char *name, int priority, proc_func_t func, void *arg) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            proc_table[i].pid        = next_pid++;
            proc_table[i].ppid       = current_pid;
            proc_table[i].state      = PROC_READY;
            proc_table[i].priority   = priority;
            proc_table[i].exit_code  = 0;
            proc_table[i].cpu_time   = 0;
            proc_table[i].start_time = time(NULL);
            proc_table[i].func       = func;
            proc_table[i].arg        = arg;
            strncpy(proc_table[i].name, name, 31);
            return proc_table[i].pid;
        }
    }
    return -1;
}

static int pm_kill(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_UNUSED) {
            if (pid <= 1) { printf("Cannot kill system process.\n"); return -1; }
            proc_table[i].state = PROC_ZOMBIE;
            return 0;
        }
    }
    return -1;
}

static void pm_reap_zombies(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_ZOMBIE) {
            proc_table[i].state = PROC_UNUSED;
        }
    }
}

static const char *proc_state_str(int state) {
    switch (state) {
        case PROC_UNUSED:  return "UNUSED";
        case PROC_READY:   return "READY";
        case PROC_RUNNING: return "RUNNING";
        case PROC_SLEEPING:return "SLEEP";
        case PROC_ZOMBIE:  return "ZOMBIE";
        default:           return "?";
    }
}

static void pm_list(void) {
    printf(CLR_CYAN "\n%-6s %-6s %-10s %-8s %-10s %s\n" CLR_RESET,
           "PID", "PPID", "NAME", "PRI", "STATE", "CPU");
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_UNUSED) {
            printf("%-6d %-6d %-10s %-8d %-10s %llu\n",
                   proc_table[i].pid,
                   proc_table[i].ppid,
                   proc_table[i].name,
                   proc_table[i].priority,
                   proc_state_str(proc_table[i].state),
                   (unsigned long long)proc_table[i].cpu_time);
        }
    }
    printf("\n");
}

/* Round-robin scheduler tick simulation */
static void pm_tick(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_READY ||
            proc_table[i].state == PROC_RUNNING) {
            proc_table[i].cpu_time += (uint64_t)(proc_table[i].priority + 1);
        }
    }
}

/* =========================================================
 * VIRTUAL FILE SYSTEM
 * ========================================================= */

typedef struct {
    int     used;
    char    name[MAX_FILENAME];
    char    path[MAX_PATH];
    uint8_t data[MAX_FILE_SIZE];
    size_t  size;
    int     perms;
    time_t  ctime;
    time_t  mtime;
} VFile;

static VFile    vfs[MAX_FILES];
static char     cwd[MAX_PATH] = "/";

static void vfs_init(void) {
    memset(vfs, 0, sizeof(vfs));
    /* Create some default files */
    int idx = 0;

    vfs[idx].used  = 1;
    vfs[idx].perms = PERM_READ;
    vfs[idx].ctime = time(NULL);
    vfs[idx].mtime = time(NULL);
    strncpy(vfs[idx].name, "readme.txt", MAX_FILENAME-1);
    strncpy(vfs[idx].path, "/readme.txt", MAX_PATH-1);
    const char *motd = "Welcome to MiniOS v1.0.0\nA simulated OS shell in C.\n";
    memcpy(vfs[idx].data, motd, strlen(motd));
    vfs[idx].size = strlen(motd);
    idx++;

    vfs[idx].used  = 1;
    vfs[idx].perms = PERM_READ | PERM_WRITE;
    vfs[idx].ctime = time(NULL);
    vfs[idx].mtime = time(NULL);
    strncpy(vfs[idx].name, "motd", MAX_FILENAME-1);
    strncpy(vfs[idx].path, "/motd", MAX_PATH-1);
    const char *info = "Message of the day: Keep hacking!\n";
    memcpy(vfs[idx].data, info, strlen(info));
    vfs[idx].size = strlen(info);
}

static VFile *vfs_find(const char *path) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (vfs[i].used && strcmp(vfs[i].path, path) == 0)
            return &vfs[i];
    }
    return NULL;
}

static int vfs_create(const char *path, int perms) {
    if (vfs_find(path)) return -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!vfs[i].used) {
            vfs[i].used  = 1;
            vfs[i].size  = 0;
            vfs[i].perms = perms;
            vfs[i].ctime = time(NULL);
            vfs[i].mtime = time(NULL);
            strncpy(vfs[i].path, path, MAX_PATH-1);
            /* Extract name from path */
            const char *slash = strrchr(path, '/');
            strncpy(vfs[i].name, slash ? slash+1 : path, MAX_FILENAME-1);
            return i;
        }
    }
    return -1;
}

static int vfs_write(const char *path, const uint8_t *data, size_t len) {
    VFile *f = vfs_find(path);
    if (!f) return -1;
    if (!(f->perms & PERM_WRITE)) { printf("Permission denied.\n"); return -1; }
    if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;
    memcpy(f->data, data, len);
    f->size  = len;
    f->mtime = time(NULL);
    return (int)len;
}

static int vfs_append(const char *path, const uint8_t *data, size_t len) {
    VFile *f = vfs_find(path);
    if (!f) return -1;
    if (!(f->perms & PERM_WRITE)) { printf("Permission denied.\n"); return -1; }
    size_t avail = MAX_FILE_SIZE - f->size;
    if (len > avail) len = avail;
    memcpy(f->data + f->size, data, len);
    f->size  += len;
    f->mtime  = time(NULL);
    return (int)len;
}

static int vfs_delete(const char *path) {
    VFile *f = vfs_find(path);
    if (!f) return -1;
    memset(f, 0, sizeof(VFile));
    return 0;
}

static void vfs_ls(const char *dir) {
    printf(CLR_CYAN "\n%-20s %-6s %-8s %s\n" CLR_RESET, "NAME", "PERMS", "SIZE", "MODIFIED");
    printf("----------------------------------------------\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (!vfs[i].used) continue;
        /* Simple dir filter: file's path starts with dir */
        if (strncmp(vfs[i].path, dir, strlen(dir)) != 0) continue;
        char perm_str[4] = "---";
        if (vfs[i].perms & PERM_READ)  perm_str[0] = 'r';
        if (vfs[i].perms & PERM_WRITE) perm_str[1] = 'w';
        if (vfs[i].perms & PERM_EXEC)  perm_str[2] = 'x';
        char tbuf[32];
        struct tm *tm_info = localtime(&vfs[i].mtime);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm_info);
        printf("%-20s %-6s %-8zu %s\n",
               vfs[i].name, perm_str, vfs[i].size, tbuf);
    }
    printf("\n");
}

/* =========================================================
 * ENVIRONMENT VARIABLES
 * ========================================================= */

typedef struct {
    char key[MAX_ENV_KEY];
    char val[MAX_ENV_VAL];
    int  used;
} EnvVar;

static EnvVar env_store[MAX_ENV_VARS];

static void env_init(void) {
    memset(env_store, 0, sizeof(env_store));
    /* Set some defaults */
    int i = 0;
    strncpy(env_store[i].key, "PATH",    MAX_ENV_KEY-1);
    strncpy(env_store[i].val, "/bin:/usr/bin", MAX_ENV_VAL-1);
    env_store[i].used = 1; i++;

    strncpy(env_store[i].key, "HOME",    MAX_ENV_KEY-1);
    strncpy(env_store[i].val, "/home/user", MAX_ENV_VAL-1);
    env_store[i].used = 1; i++;

    strncpy(env_store[i].key, "SHELL",   MAX_ENV_KEY-1);
    strncpy(env_store[i].val, "/bin/minish", MAX_ENV_VAL-1);
    env_store[i].used = 1; i++;

    strncpy(env_store[i].key, "USER",    MAX_ENV_KEY-1);
    strncpy(env_store[i].val, "root", MAX_ENV_VAL-1);
    env_store[i].used = 1; i++;

    strncpy(env_store[i].key, "TERM",    MAX_ENV_KEY-1);
    strncpy(env_store[i].val, "xterm-256color", MAX_ENV_VAL-1);
    env_store[i].used = 1;
}

static const char *env_get(const char *key) {
    for (int i = 0; i < MAX_ENV_VARS; i++)
        if (env_store[i].used && strcmp(env_store[i].key, key) == 0)
            return env_store[i].val;
    return NULL;
}

static int env_set(const char *key, const char *val) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_store[i].used && strcmp(env_store[i].key, key) == 0) {
            strncpy(env_store[i].val, val, MAX_ENV_VAL-1);
            return 0;
        }
    }
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (!env_store[i].used) {
            env_store[i].used = 1;
            strncpy(env_store[i].key, key, MAX_ENV_KEY-1);
            strncpy(env_store[i].val, val, MAX_ENV_VAL-1);
            return 0;
        }
    }
    return -1;
}

static int env_unset(const char *key) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_store[i].used && strcmp(env_store[i].key, key) == 0) {
            memset(&env_store[i], 0, sizeof(EnvVar));
            return 0;
        }
    }
    return -1;
}

static void env_print_all(void) {
    for (int i = 0; i < MAX_ENV_VARS; i++)
        if (env_store[i].used)
            printf("  %s=%s\n", env_store[i].key, env_store[i].val);
}

/* =========================================================
 * COMMAND HISTORY
 * ========================================================= */

static char history[MAX_HISTORY][MAX_CMD_LEN];
static int  history_count = 0;

static void history_push(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count++], cmd, MAX_CMD_LEN-1);
    } else {
        /* Shift */
        memmove(history[0], history[1], sizeof(char) * MAX_CMD_LEN * (MAX_HISTORY-1));
        strncpy(history[MAX_HISTORY-1], cmd, MAX_CMD_LEN-1);
    }
}

static void history_print(void) {
    printf(CLR_CYAN "\n--- Command History ---\n" CLR_RESET);
    for (int i = 0; i < history_count; i++)
        printf("  %3d  %s\n", i+1, history[i]);
    printf("\n");
}

/* =========================================================
 * UTILITY FUNCTIONS
 * ========================================================= */

static char *str_trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end+1) = '\0';
    return s;
}

static int str_split(char *s, char **argv, int max_args) {
    int argc = 0;
    char *p = s;
    while (*p && argc < max_args) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        /* Handle quoted strings */
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void print_banner(void) {
    printf(CLR_BOLD CLR_CYAN
        "\n"
        "  ███╗   ███╗██╗███╗   ██╗██╗ ██████╗ ███████╗\n"
        "  ████╗ ████║██║████╗  ██║██║██╔═══██╗██╔════╝\n"
        "  ██╔████╔██║██║██╔██╗ ██║██║██║   ██║███████╗\n"
        "  ██║╚██╔╝██║██║██║╚██╗██║██║██║   ██║╚════██║\n"
        "  ██║ ╚═╝ ██║██║██║ ╚████║██║╚██████╔╝███████║\n"
        "  ╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝ ╚═════╝ ╚══════╝\n"
        CLR_RESET
        CLR_YELLOW
        "  Version " OS_VERSION " | A Mini OS Simulator in C\n"
        "  Type 'help' for available commands.\n\n"
        CLR_RESET);
}

static void print_help(void) {
    printf(CLR_BOLD "\nMiniOS Shell Commands\n" CLR_RESET);
    printf("--------------------------------------------------------------\n");
    printf(CLR_YELLOW "  System:\n" CLR_RESET);
    printf("    help              Show this help message\n");
    printf("    uname             Print OS info\n");
    printf("    uptime            Show system uptime\n");
    printf("    clear             Clear screen\n");
    printf("    exit / quit       Exit MiniOS\n");
    printf(CLR_YELLOW "\n  Memory Manager:\n" CLR_RESET);
    printf("    meminfo           Show heap memory stats and map\n");
    printf("    malloc <size>     Allocate <size> bytes (test)\n");
    printf("    mfree <addr>      Free memory at address\n");
    printf(CLR_YELLOW "\n  Process Manager:\n" CLR_RESET);
    printf("    ps                List all processes\n");
    printf("    spawn <name> <pri> Spawn a new process\n");
    printf("    kill <pid>        Kill a process\n");
    printf("    tick              Advance scheduler tick\n");
    printf(CLR_YELLOW "\n  File System:\n" CLR_RESET);
    printf("    ls [dir]          List files\n");
    printf("    cat <file>        Print file content\n");
    printf("    touch <file>      Create empty file\n");
    printf("    write <file> <txt> Write text to file\n");
    printf("    append <file> <txt> Append text to file\n");
    printf("    rm <file>         Delete a file\n");
    printf("    pwd               Print working directory\n");
    printf("    cd <path>         Change directory\n");
    printf(CLR_YELLOW "\n  Environment:\n" CLR_RESET);
    printf("    env               Print all env vars\n");
    printf("    export KEY=VAL    Set env variable\n");
    printf("    unset KEY         Remove env variable\n");
    printf("    echo <text>       Print text (expands $VAR)\n");
    printf(CLR_YELLOW "\n  Misc:\n" CLR_RESET);
    printf("    history           Show command history\n");
    printf("    date              Print current date/time\n");
    printf("    whoami            Print current user\n");
    printf("    calc <expr>       Simple integer calculator\n");
    printf("    sleep <secs>      Sleep for N seconds (simulated)\n");
    printf("\n");
}

/* =========================================================
 * SIMPLE CALCULATOR PARSER
 * ========================================================= */

typedef struct {
    const char *src;
    int         pos;
} CalcState;

static long long calc_parse_expr(CalcState *s);

static void calc_skip_ws(CalcState *s) {
    while (s->src[s->pos] == ' ' || s->src[s->pos] == '\t') s->pos++;
}

static long long calc_parse_num(CalcState *s) {
    calc_skip_ws(s);
    int neg = 0;
    if (s->src[s->pos] == '-') { neg = 1; s->pos++; }
    if (s->src[s->pos] == '(') {
        s->pos++;
        long long val = calc_parse_expr(s);
        calc_skip_ws(s);
        if (s->src[s->pos] == ')') s->pos++;
        return neg ? -val : val;
    }
    long long val = 0;
    int digits = 0;
    while (isdigit((unsigned char)s->src[s->pos])) {
        val = val * 10 + (s->src[s->pos] - '0');
        s->pos++;
        digits++;
    }
    if (!digits) { printf("  Syntax error\n"); return 0; }
    return neg ? -val : val;
}

static long long calc_parse_term(CalcState *s) {
    long long val = calc_parse_num(s);
    for (;;) {
        calc_skip_ws(s);
        char op = s->src[s->pos];
        if (op == '*' || op == '/' || op == '%') {
            s->pos++;
            long long rhs = calc_parse_num(s);
            if (op == '*')      val *= rhs;
            else if (op == '/') val = rhs ? val / rhs : 0;
            else                val = rhs ? val % rhs : 0;
        } else break;
    }
    return val;
}

static long long calc_parse_expr(CalcState *s) {
    long long val = calc_parse_term(s);
    for (;;) {
        calc_skip_ws(s);
        char op = s->src[s->pos];
        if (op == '+' || op == '-') {
            s->pos++;
            long long rhs = calc_parse_term(s);
            val = (op == '+') ? val + rhs : val - rhs;
        } else break;
    }
    return val;
}

static long long calc_eval(const char *expr) {
    CalcState s = { expr, 0 };
    return calc_parse_expr(&s);
}

/* =========================================================
 * ECHO WITH ENV EXPANSION
 * ========================================================= */

static void do_echo(const char *text) {
    char out[MAX_CMD_LEN * 2] = {0};
    int  oi = 0;
    int  ti = 0;
    int  len = (int)strlen(text);

    while (ti < len && oi < (int)sizeof(out)-1) {
        if (text[ti] == '$') {
            ti++;
            char key[MAX_ENV_KEY] = {0};
            int ki = 0;
            while (ti < len && (isalnum((unsigned char)text[ti]) || text[ti] == '_') && ki < MAX_ENV_KEY-1)
                key[ki++] = text[ti++];
            const char *val = env_get(key);
            if (val) {
                int vl = (int)strlen(val);
                if (oi + vl < (int)sizeof(out)-1) {
                    memcpy(out+oi, val, vl);
                    oi += vl;
                }
            }
        } else {
            out[oi++] = text[ti++];
        }
    }
    out[oi] = '\0';
    printf("%s\n", out);
}

/* =========================================================
 * SHELL: MAIN COMMAND DISPATCHER
 * ========================================================= */

static time_t boot_time;

/* We'll store a handful of test malloc pointers */
#define MAX_TEST_ALLOCS 16
static void   *test_allocs[MAX_TEST_ALLOCS];
static int     test_alloc_count = 0;

static int run_command(char *line) {
    char *args[MAX_ARGS];
    int   argc = str_split(line, args, MAX_ARGS);
    if (argc == 0) return 0;
    const char *cmd = args[0];

    /* ---- SYSTEM ---- */

    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        printf(CLR_YELLOW "Goodbye from MiniOS!\n" CLR_RESET);
        return -1;
    }

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(cmd, "clear") == 0) {
        printf("\033[2J\033[H");
        return 0;
    }

    if (strcmp(cmd, "uname") == 0) {
        printf("%s %s (virtual) C-kernel #1 SMP\n", OS_NAME, OS_VERSION);
        return 0;
    }

    if (strcmp(cmd, "uptime") == 0) {
        time_t now  = time(NULL);
        double secs = difftime(now, boot_time);
        int h = (int)secs / 3600;
        int m = ((int)secs % 3600) / 60;
        int s = (int)secs % 60;
        printf("Uptime: %02d:%02d:%02d\n", h, m, s);
        return 0;
    }

    if (strcmp(cmd, "date") == 0) {
        time_t now = time(NULL);
        printf("%s", ctime(&now));
        return 0;
    }

    if (strcmp(cmd, "whoami") == 0) {
        const char *user = env_get("USER");
        printf("%s\n", user ? user : "unknown");
        return 0;
    }

    if (strcmp(cmd, "sleep") == 0) {
        int secs = (argc > 1) ? atoi(args[1]) : 1;
        printf("Simulated sleep for %d second(s).\n", secs);
        /* In real OS we'd block; here we just simulate */
        return 0;
    }

    /* ---- MEMORY ---- */

    if (strcmp(cmd, "meminfo") == 0) {
        mm_dump();
        return 0;
    }

    if (strcmp(cmd, "malloc") == 0) {
        if (argc < 2) { printf("Usage: malloc <size>\n"); return 0; }
        size_t sz = (size_t)atoi(args[1]);
        if (test_alloc_count >= MAX_TEST_ALLOCS) {
            printf("Test alloc table full. Free some with mfree.\n");
            return 0;
        }
        void *p = mm_malloc(sz);
        if (!p) { printf(CLR_RED "Allocation failed (out of memory).\n" CLR_RESET); return 0; }
        /* Fill with pattern */
        memset(p, 0xAB, sz);
        test_allocs[test_alloc_count++] = p;
        printf("Allocated %zu bytes at %p (index %d)\n", sz, p, test_alloc_count-1);
        return 0;
    }

    if (strcmp(cmd, "mfree") == 0) {
        if (argc < 2) { printf("Usage: mfree <index>\n"); return 0; }
        int idx = atoi(args[1]);
        if (idx < 0 || idx >= test_alloc_count || !test_allocs[idx]) {
            printf("Invalid index.\n"); return 0;
        }
        mm_free(test_allocs[idx]);
        printf("Freed allocation at index %d (%p).\n", idx, test_allocs[idx]);
        test_allocs[idx] = NULL;
        return 0;
    }

    /* ---- PROCESS ---- */

    if (strcmp(cmd, "ps") == 0) {
        pm_list();
        return 0;
    }

    if (strcmp(cmd, "spawn") == 0) {
        const char *name = (argc > 1) ? args[1] : "proc";
        int   pri  = (argc > 2) ? atoi(args[2]) : 1;
        int   pid  = pm_spawn(name, pri, NULL, NULL);
        if (pid < 0) printf("Failed to spawn process.\n");
        else         printf("Spawned process '%s' with PID %d.\n", name, pid);
        return 0;
    }

    if (strcmp(cmd, "kill") == 0) {
        if (argc < 2) { printf("Usage: kill <pid>\n"); return 0; }
        int pid = atoi(args[1]);
        if (pm_kill(pid) == 0) printf("Sent SIGTERM to PID %d.\n", pid);
        else                   printf("No such process: %d\n", pid);
        pm_reap_zombies();
        return 0;
    }

    if (strcmp(cmd, "tick") == 0) {
        int n = (argc > 1) ? atoi(args[1]) : 1;
        for (int i = 0; i < n; i++) pm_tick();
        printf("Advanced %d scheduler tick(s).\n", n);
        return 0;
    }

    /* ---- FILE SYSTEM ---- */

    if (strcmp(cmd, "pwd") == 0) {
        printf("%s\n", cwd);
        return 0;
    }

    if (strcmp(cmd, "cd") == 0) {
        const char *dest = (argc > 1) ? args[1] : "/";
        strncpy(cwd, dest, MAX_PATH-1);
        printf("Changed directory to %s\n", cwd);
        return 0;
    }

    if (strcmp(cmd, "ls") == 0) {
        const char *dir = (argc > 1) ? args[1] : "/";
        vfs_ls(dir);
        return 0;
    }

    if (strcmp(cmd, "cat") == 0) {
        if (argc < 2) { printf("Usage: cat <file>\n"); return 0; }
        /* Build absolute path */
        char path[MAX_PATH];
        if (args[1][0] == '/') snprintf(path, MAX_PATH, "%s", args[1]);
        else snprintf(path, MAX_PATH, "%s%s%s", cwd,
                      cwd[strlen(cwd)-1] == '/' ? "" : "/", args[1]);
        VFile *f = vfs_find(path);
        if (!f) { printf("cat: %s: No such file\n", args[1]); return 0; }
        if (!(f->perms & PERM_READ)) { printf("Permission denied.\n"); return 0; }
        fwrite(f->data, 1, f->size, stdout);
        if (f->size && f->data[f->size-1] != '\n') printf("\n");
        return 0;
    }

    if (strcmp(cmd, "touch") == 0) {
        if (argc < 2) { printf("Usage: touch <file>\n"); return 0; }
        char path[MAX_PATH];
        if (args[1][0] == '/') snprintf(path, MAX_PATH, "%s", args[1]);
        else snprintf(path, MAX_PATH, "%s%s%s", cwd,
                      cwd[strlen(cwd)-1] == '/' ? "" : "/", args[1]);
        if (vfs_find(path)) { printf("File already exists.\n"); return 0; }
        if (vfs_create(path, PERM_READ | PERM_WRITE) >= 0)
            printf("Created: %s\n", path);
        else
            printf("Failed to create file (limit reached).\n");
        return 0;
    }

    if (strcmp(cmd, "write") == 0) {
        if (argc < 3) { printf("Usage: write <file> <text>\n"); return 0; }
        char path[MAX_PATH];
        if (args[1][0] == '/') snprintf(path, MAX_PATH, "%s", args[1]);
        else snprintf(path, MAX_PATH, "%s%s%s", cwd,
                      cwd[strlen(cwd)-1] == '/' ? "" : "/", args[1]);
        /* Recreate if missing */
        if (!vfs_find(path)) vfs_create(path, PERM_READ | PERM_WRITE);
        /* Join remaining args */
        char text[MAX_FILE_SIZE] = {0};
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(text, " ", MAX_FILE_SIZE - strlen(text) - 1);
            strncat(text, args[i], MAX_FILE_SIZE - strlen(text) - 1);
        }
        strncat(text, "\n", MAX_FILE_SIZE - strlen(text) - 1);
        vfs_write(path, (uint8_t *)text, strlen(text));
        printf("Written %zu bytes to %s\n", strlen(text), path);
        return 0;
    }

    if (strcmp(cmd, "append") == 0) {
        if (argc < 3) { printf("Usage: append <file> <text>\n"); return 0; }
        char path[MAX_PATH];
        if (args[1][0] == '/') snprintf(path, MAX_PATH, "%s", args[1]);
        else snprintf(path, MAX_PATH, "%s%s%s", cwd,
                      cwd[strlen(cwd)-1] == '/' ? "" : "/", args[1]);
        if (!vfs_find(path)) vfs_create(path, PERM_READ | PERM_WRITE);
        char text[MAX_FILE_SIZE] = {0};
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(text, " ", MAX_FILE_SIZE - strlen(text) - 1);
            strncat(text, args[i], MAX_FILE_SIZE - strlen(text) - 1);
        }
        strncat(text, "\n", MAX_FILE_SIZE - strlen(text) - 1);
        vfs_append(path, (uint8_t *)text, strlen(text));
        printf("Appended %zu bytes to %s\n", strlen(text), path);
        return 0;
    }

    if (strcmp(cmd, "rm") == 0) {
        if (argc < 2) { printf("Usage: rm <file>\n"); return 0; }
        char path[MAX_PATH];
        if (args[1][0] == '/') snprintf(path, MAX_PATH, "%s", args[1]);
        else snprintf(path, MAX_PATH, "%s%s%s", cwd,
                      cwd[strlen(cwd)-1] == '/' ? "" : "/", args[1]);
        if (vfs_delete(path) == 0) printf("Removed: %s\n", path);
        else                       printf("rm: %s: No such file\n", args[1]);
        return 0;
    }

    /* ---- ENVIRONMENT ---- */

    if (strcmp(cmd, "env") == 0) {
        env_print_all();
        return 0;
    }

    if (strcmp(cmd, "export") == 0) {
        if (argc < 2) { printf("Usage: export KEY=VALUE\n"); return 0; }
        char *eq = strchr(args[1], '=');
        if (!eq) { printf("Invalid format. Use KEY=VALUE\n"); return 0; }
        *eq = '\0';
        env_set(args[1], eq+1);
        printf("Set %s=%s\n", args[1], eq+1);
        return 0;
    }

    if (strcmp(cmd, "unset") == 0) {
        if (argc < 2) { printf("Usage: unset KEY\n"); return 0; }
        if (env_unset(args[1]) == 0) printf("Unset %s\n", args[1]);
        else                         printf("Variable not found: %s\n", args[1]);
        return 0;
    }

    if (strcmp(cmd, "echo") == 0) {
        /* Re-join from original line after "echo " */
        const char *rest = strstr(line, "echo");
        if (rest) { rest += 4; while (*rest == ' ') rest++; }
        do_echo(rest ? rest : "");
        return 0;
    }

    /* ---- MISC ---- */

    if (strcmp(cmd, "history") == 0) {
        history_print();
        return 0;
    }

    if (strcmp(cmd, "calc") == 0) {
        if (argc < 2) { printf("Usage: calc <expression>\n"); return 0; }
        /* Re-join args into expression */
        char expr[MAX_CMD_LEN] = {0};
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(expr, " ", MAX_CMD_LEN - strlen(expr) - 1);
            strncat(expr, args[i], MAX_CMD_LEN - strlen(expr) - 1);
        }
        long long result = calc_eval(expr);
        printf("  = %lld\n", result);
        return 0;
    }

    /* Unknown command */
    printf(CLR_RED "minios: command not found: %s\n" CLR_RESET, cmd);
    return 0;
}

/* =========================================================
 * SIGNAL HANDLER
 * ========================================================= */

static volatile int got_sigint = 0;

static void handle_sigint(int sig) {
    (void)sig;
    got_sigint = 1;
    printf("\n" CLR_YELLOW "[MiniOS] SIGINT received. Type 'exit' to quit.\n" CLR_RESET);
}

/* =========================================================
 * MAIN ENTRY POINT
 * ========================================================= */

int main(void) {
    /* Boot sequence */
    boot_time = time(NULL);
    signal(SIGINT, handle_sigint);

    mm_init();
    pm_init();
    vfs_init();
    env_init();
    memset(test_allocs, 0, sizeof(test_allocs));

    print_banner();

    char line[MAX_CMD_LEN];

    while (1) {
        /* Show prompt */
        printf(CLR_BOLD CLR_GREEN "%s" CLR_RESET CLR_BOLD "%s" CLR_RESET " ",
               env_get("USER"), SHELL_PROMPT);
        fflush(stdout);

        got_sigint = 0;

        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            clearerr(stdin);
            continue;
        }

        if (got_sigint) continue;

        /* Strip newline */
        size_t llen = strlen(line);
        if (llen > 0 && line[llen-1] == '\n') line[llen-1] = '\0';

        char *trimmed = str_trim(line);
        if (*trimmed == '\0') continue;
        if (*trimmed == '#')  continue;  /* comments */

        history_push(trimmed);

        /* Make a mutable copy for parser */
        char cmd_copy[MAX_CMD_LEN];
        strncpy(cmd_copy, trimmed, MAX_CMD_LEN-1);

        int ret = run_command(cmd_copy);
        if (ret < 0) break;

        /* Periodic scheduler tick on every command */
        pm_tick();
    }

    /* Cleanup */
    printf(CLR_CYAN "MiniOS shutdown. Uptime: " CLR_RESET);
    time_t now   = time(NULL);
    double secs  = difftime(now, boot_time);
    int h = (int)secs / 3600;
    int m = ((int)secs % 3600) / 60;
    int s = (int)secs % 60;
    printf("%02d:%02d:%02d\n\n", h, m, s);

    return 0;
}

/* =========================================================
#include "beta.c"
