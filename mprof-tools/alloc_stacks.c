// mprof-tools/alloc_stacks.c
#define _GNU_SOURCE
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "alloc_stacks.skel.h"

#define MAX_STACK_DEPTH 127

typedef __u32 u32;
typedef __u64 u64;

static volatile sig_atomic_t exiting = 0;
static void sig_handler(int sig) { (void)sig; exiting = 1; }

struct agg { u64 count; u64 bytes; };

struct item {
    u32 sid;
    u64 count;
    u64 bytes;
};

/* ----------------------------
 * /proc/<pid>/maps parsing
 * ---------------------------- */

struct map_ent {
    u64 start, end;
    u64 fileoff;
    char perms[5];
    char path[256];  // enough for typical paths
};

struct maps_db {
    struct map_ent *ents;
    size_t n, cap;
};

static void maps_db_free(struct maps_db *db) {
    free(db->ents);
    db->ents = NULL;
    db->n = db->cap = 0;
}

static int maps_db_push(struct maps_db *db, const struct map_ent *e) {
    if (db->n == db->cap) {
        size_t nc = db->cap ? db->cap * 2 : 128;
        void *p = realloc(db->ents, nc * sizeof(db->ents[0]));
        if (!p) return -1;
        db->ents = p;
        db->cap = nc;
    }
    db->ents[db->n++] = *e;
    return 0;
}

static int read_proc_maps(int pid, struct maps_db *db) {
    maps_db_free(db);

    char p[64];
    snprintf(p, sizeof p, "/proc/%d/maps", pid);
    FILE *f = fopen(p, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        struct map_ent e;
        memset(&e, 0, sizeof e);

        // Typical line:
        // start-end perms offset dev inode pathname...
        // pathname may be absent
        char pathbuf[256] = {0};
        int n = sscanf(line,
                       "%llx-%llx %4s %llx %*s %*s %255[^\n]",
                       (unsigned long long *)&e.start,
                       (unsigned long long *)&e.end,
                       e.perms,
                       (unsigned long long *)&e.fileoff,
                       pathbuf);
        if (n < 4) continue;

        if (n >= 5) {
            // trim leading spaces
            char *s = pathbuf;
            while (*s == ' ') s++;
            strncpy(e.path, s, sizeof e.path - 1);
        } else {
            strcpy(e.path, "");
        }

        // We care mostly about executable mappings for symbolization
        if (maps_db_push(db, &e) != 0) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

static const struct map_ent *find_map_for_ip(const struct maps_db *db, u64 ip) {
    for (size_t i = 0; i < db->n; i++) {
        const struct map_ent *e = &db->ents[i];
        if (ip >= e->start && ip < e->end) {
            return e;
        }
    }
    return NULL;
}

/* ----------------------------
 * addr2line symbolization
 * ---------------------------- */

static int symbolize_ip(const struct map_ent *m, u64 ip, char *out, size_t out_sz) {
    if (!m) {
        snprintf(out, out_sz, "<no-map>");
        return -1;
    }
    if (m->path[0] == '\0' || m->path[0] == '[') {
        // [vdso], [stack], anon, etc.
        snprintf(out, out_sz, "%s", (m->path[0] ? m->path : "<anon>"));
        return -1;
    }

    // Compute ELF-relative address
    u64 elf_addr = (ip - m->start) + m->fileoff;

    // Run addr2line
    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "addr2line -f -p -e '%s' 0x%llx 2>/dev/null",
             m->path, (unsigned long long)elf_addr);

    FILE *pp = popen(cmd, "r");
    if (!pp) {
        snprintf(out, out_sz, "<addr2line failed>");
        return -1;
    }

    if (!fgets(out, (int)out_sz, pp)) {
        pclose(pp);
        snprintf(out, out_sz, "<no symbol>");
        return -1;
    }
    pclose(pp);

    // strip newline
    size_t L = strlen(out);
    if (L && out[L-1] == '\n') out[L-1] = '\0';

    return 0;
}

/* ----------------------------
 * sorting
 * ---------------------------- */

static int cmp_bytes_desc(const void *a, const void *b) {
    const struct item *x = a, *y = b;
    if (y->bytes > x->bytes) return 1;
    if (y->bytes < x->bytes) return -1;
    return 0;
}

/* ----------------------------
 * main
 * ---------------------------- */

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <PID> <PATH_TO_LIBC>\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    const char *libc_path = argv[2];

    struct alloc_stacks_bpf *skel = alloc_stacks_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open skeleton\n");
        return 1;
    }

    if (alloc_stacks_bpf__load(skel)) {
        fprintf(stderr, "Failed to load BPF program\n");
        alloc_stacks_bpf__destroy(skel);
        return 1;
    }

    printf("Attaching to USDT 'memory_prof_sample' in %s (PID %d)...\n", libc_path, pid);

    skel->links.trace_alloc =
        bpf_program__attach_usdt(
            skel->progs.trace_alloc,
            pid,
            libc_path,
            "libc",
            "memory_prof_sample",
            NULL);

    if (!skel->links.trace_alloc) {
        perror("Failed to attach USDT");
        alloc_stacks_bpf__destroy(skel);
        return 1;
    }

    signal(SIGINT, sig_handler);
    printf("Ctrl+C to stop.\n");

    int stacks_fd   = bpf_map__fd(skel->maps.stacks);
    int stackmap_fd = bpf_map__fd(skel->maps.stackmap);
    int ok_fd       = bpf_map__fd(skel->maps.seen_ok);

    struct maps_db maps = {0};
    if (read_proc_maps(pid, &maps) != 0) {
        fprintf(stderr, "Warning: failed to read /proc/%d/maps; symbolization will be limited\n", pid);
    }

    while (!exiting) {
        sleep(2);

        // refresh maps occasionally in case ASLR changes mappings (not common during run, but safe)
        read_proc_maps(pid, &maps);

        u32 k0 = 0;
        u64 ok = 0;
        bpf_map_lookup_elem(ok_fd, &k0, &ok);

        struct item top[10] = {0};
        int topn = 0;

        u32 key = 0, next_key;

        while (bpf_map_get_next_key(stacks_fd, &key, &next_key) == 0) {
            int ncpu = libbpf_num_possible_cpus();
            struct agg *percpu = calloc(ncpu, sizeof(*percpu));
            if (!percpu) break;

            if (bpf_map_lookup_elem(stacks_fd, &next_key, percpu) == 0) {
                u64 bytes = 0, count = 0;
                for (int i = 0; i < ncpu; i++) {
                    bytes += percpu[i].bytes;
                    count += percpu[i].count;
                }

                struct item it = { .sid = next_key, .bytes = bytes, .count = count };

                if (topn < 10) {
                    top[topn++] = it;
                } else {
                    int mi = 0;
                    for (int i = 1; i < 10; i++)
                        if (top[i].bytes < top[mi].bytes) mi = i;
                    if (it.bytes > top[mi].bytes) top[mi] = it;
                }
            }

            free(percpu);
            key = next_key;
        }

        qsort(top, topn, sizeof(top[0]), cmp_bytes_desc);

        printf("\033[2J\033[H");
        printf("=== Top allocation stacks (by bytes, cumulative) ===\n");
        printf("seen_ok (sane sizes): %llu\n\n", (unsigned long long)ok);

        for (int i = 0; i < topn; i++) {
            u64 avg = top[i].bytes / (top[i].count ? top[i].count : 1);

            printf("#%d  bytes=%llu  count=%llu  avg=%llu  stack_id=%u\n",
                   i + 1,
                   (unsigned long long)top[i].bytes,
                   (unsigned long long)top[i].count,
                   (unsigned long long)avg,
                   top[i].sid);

            u64 ips[MAX_STACK_DEPTH] = {0};
            if (bpf_map_lookup_elem(stackmap_fd, &top[i].sid, ips) == 0) {
                for (int f = 0; f < 20; f++) {
                    if (!ips[f]) break;

                    const struct map_ent *m = find_map_for_ip(&maps, ips[f]);
                    char sym[256];
                    symbolize_ip(m, ips[f], sym, sizeof sym);

                    if (m && m->path[0]) {
                        printf("    [%02d] 0x%llx  %s  (%s)\n",
                               f, (unsigned long long)ips[f],
                               sym, m->path);
                    } else {
                        printf("    [%02d] 0x%llx  %s\n",
                               f, (unsigned long long)ips[f], sym);
                    }
                }
            }
            printf("\n");
        }

        fflush(stdout);
    }

    maps_db_free(&maps);
    alloc_stacks_bpf__destroy(skel);
    return 0;
}