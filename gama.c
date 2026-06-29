 * EXTENDED SHELL COMMANDS (for klog, cron, net, syscall, pipe)
 * These are registered as extras — call from a wrapper main
 * or integrate into run_command() for a full build.
 * ========================================================= */

/*
 * run_extended_command() – call this BEFORE run_command() falls
 * through to "command not found" to handle the extra built-ins.
 * Returns 1 if handled, 0 if not recognized.
 */
static int run_extended_command(const char *orig_line) {
    char line_copy[MAX_CMD_LEN];
    strncpy(line_copy, orig_line, MAX_CMD_LEN-1);
    char *args[MAX_ARGS];
    int   argc = str_split(line_copy, args, MAX_ARGS);
    if (argc == 0) return 0;
    const char *cmd = args[0];

    /* --- klog --- */
    if (strcmp(cmd, "klog") == 0) {
        int n = (argc > 1) ? atoi(args[1]) : 20;
        klog_dump(n);
        return 1;
    }

    if (strcmp(cmd, "klog-add") == 0) {
        if (argc < 3) { printf("Usage: klog-add <level: 0-3> <message>\n"); return 1; }
        KLogLevel lvl = (KLogLevel)atoi(args[1]);
        char msg[KLOG_MSG_LEN] = {0};
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(msg, " ", KLOG_MSG_LEN-strlen(msg)-1);
            strncat(msg, args[i], KLOG_MSG_LEN-strlen(msg)-1);
        }
        klog_write(lvl, "%s", msg);
        printf("Logged: [%s] %s\n", klog_level_str(lvl), msg);
        return 1;
    }

    /* --- cron --- */
    if (strcmp(cmd, "crontab") == 0) {
        if (argc < 2) { cron_list(); return 1; }
        if (strcmp(args[1], "-l") == 0) { cron_list(); return 1; }
        if (strcmp(args[1], "-r") == 0) {
            if (argc < 3) { printf("Usage: crontab -r <id>\n"); return 1; }
            int id = atoi(args[2]);
            if (cron_remove(id) == 0) printf("Removed cron job %d\n", id);
            else printf("No such cron job: %d\n", id);
            return 1;
        }
        if (strcmp(args[1], "-a") == 0) {
            if (argc < 5) { printf("Usage: crontab -a <name> <interval> <cmd>\n"); return 1; }
            int interval = atoi(args[3]);
            int id = cron_add(args[2], interval, args[4]);
            if (id > 0) printf("Added cron job %d: '%s' every %d ticks\n", id, args[2], interval);
            else printf("Failed to add cron job (limit reached).\n");
            return 1;
        }
        printf("Usage: crontab [-l | -r <id> | -a <name> <interval> <cmd>]\n");
        return 1;
    }

    if (strcmp(cmd, "crond") == 0) {
        int ticks = (argc > 1) ? atoi(args[1]) : 5;
        for (int i = 0; i < ticks; i++) cron_tick();
        printf("Ran cron daemon for %d ticks.\n", ticks);
        return 1;
    }

    /* --- net --- */
    if (strcmp(cmd, "ifconfig") == 0) {
        net_ifconfig();
        return 1;
    }

    if (strcmp(cmd, "ifset") == 0) {
        if (argc < 3) { printf("Usage: ifset <iface> <ip>\n"); return 1; }
        if (net_set_ip(args[1], args[2]) == 0)
            printf("Set %s IP to %s\n", args[1], args[2]);
        else
            printf("Interface not found: %s\n", args[1]);
        return 1;
    }

    if (strcmp(cmd, "nettraf") == 0) {
        net_simulate_traffic();
        printf("Simulated a round of network traffic.\n");
        return 1;
    }

    /* --- syscall --- */
    if (strcmp(cmd, "syscall") == 0) {
        if (argc < 2) { syscall_stats(); return 1; }
        if (strcmp(args[1], "stats") == 0) { syscall_stats(); return 1; }
        int num = atoi(args[1]);
        long a = (argc > 2) ? atol(args[2]) : 0;
        long b = (argc > 3) ? atol(args[3]) : 0;
        long c = (argc > 4) ? atol(args[4]) : 0;
        long ret = do_syscall(num, a, b, c);
        printf("syscall(%d) returned %ld\n", num, ret);
        return 1;
    }

    /* --- pipe --- */
    if (strcmp(cmd, "pipe-open") == 0) {
        int id = pipe_open();
        if (id > 0) printf("Opened pipe id=%d\n", id);
        else        printf("Failed (pipe limit reached).\n");
        return 1;
    }

    if (strcmp(cmd, "pipe-write") == 0) {
        if (argc < 3) { printf("Usage: pipe-write <id> <data>\n"); return 1; }
        int id = atoi(args[1]);
        int n  = pipe_write(id, args[2], (int)strlen(args[2]));
        if (n >= 0) printf("Wrote %d bytes to pipe %d\n", n, id);
        else        printf("Pipe not found: %d\n", id);
        return 1;
    }

    if (strcmp(cmd, "pipe-read") == 0) {
        if (argc < 2) { printf("Usage: pipe-read <id>\n"); return 1; }
        int id = atoi(args[1]);
        char buf[PIPE_BUF_SZ+1] = {0};
        int n = pipe_read(id, buf, PIPE_BUF_SZ);
        if (n >= 0) {
            buf[n] = '\0';
            printf("Read %d bytes from pipe %d: '%s'\n", n, id, buf);
        } else printf("Pipe not found: %d\n", id);
        return 1;
    }

    if (strcmp(cmd, "pipe-close") == 0) {
        if (argc < 2) { printf("Usage: pipe-close <id>\n"); return 1; }
        pipe_close(atoi(args[1]));
        printf("Closed pipe %d\n", atoi(args[1]));
        return 1;
    }

    if (strcmp(cmd, "pipes") == 0) {
        pipe_status();
        return 1;
    }

    return 0;   /* not handled */
}

/*
 * Stub initializer — call from main() if integrating the extended modules:
 *
 *   extended_init();
 *
 * and replace `run_command(cmd_copy)` with:
 *
 *   if (!run_extended_command(cmd_copy))
 *       run_command(cmd_copy);
 */
static void extended_init(void) {
    cron_init();
    net_init();
    pipe_init();
    klog_write(KLOG_INFO, OS_NAME " " OS_VERSION " booted");
    klog_write(KLOG_INFO, "mm: heap initialized (%d KB)", HEAP_SIZE / 1024);
    klog_write(KLOG_INFO, "pm: process manager ready");
    klog_write(KLOG_INFO, "vfs: virtual filesystem mounted");
    klog_write(KLOG_INFO, "net: network subsystem up");
}

/* End of extended module */
