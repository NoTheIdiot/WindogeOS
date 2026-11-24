#include "../dogec/stdio.h"
#include "../dogec/dogeio.h"
#include "../dogec/stdint.h"
#include "../dogec/string.h"

static int test_stdio_files(void) {
    const char *path = "test.test";
    const char *renamed = "test_renamed.test";
    printf("[1] Testing fopen / fwrite / fread / fseek / fgets / stat / rename / remove\n");

    remove(path);
    remove(renamed);

    FILE *fp = fopen(path, "w");
    if (!fp) {
        printf("  FAIL: fopen(\"w\") returned NULL\n");
        return -1;
    }

    const char *line1 = "PRINT TEST!\n";
    const char *line2 = "LINE TWO PRINTE TEST.\n";
    size_t len1 = strlen(line1);
    size_t len2 = strlen(line2);

    size_t w1 = fwrite(line1, 1, len1, fp);
    size_t w2 = fwrite(line2, 1, len2, fp);
    fclose(fp);

    if (w1 != len1 || w2 != len2) {
        printf("  FAIL: short write (w1=%llu, w2=%llu)\n",
               (unsigned long long)w1, (unsigned long long)w2);
        return -1;
    }

    int64_t sz = fs_size(path);
    printf("  fs_size after write: %lld (expected %llu)\n",
           (long long)sz, (unsigned long long)(len1 + len2));
    if (sz != (int64_t)(len1 + len2)) {
        printf("  FAIL: fs_size mismatch\n");
        return -1;
    }

    const char *extra = "APPENDED LINE.\n";
    if (fs_append(path, extra, strlen(extra)) != 0) {
        printf("  FAIL: fs_append failed\n");
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp) {
        printf("  FAIL: fopen(\"r\") returned NULL\n");
        return -1;
    }

    char buf1[64];
    char buf2[64];
    char buf3[64];
    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));
    memset(buf3, 0, sizeof(buf3));
    if (!fgets(buf1, sizeof(buf1), fp) ||
        !fgets(buf2, sizeof(buf2), fp) ||
        !fgets(buf3, sizeof(buf3), fp)) {
        printf("  FAIL: fgets failed\n");
        fclose(fp);
        return -1;
    }
    printf("  Read line 1: %s", buf1);
    printf("  Read line 2: %s", buf2);
    printf("  Read line 3: %s", buf3);

    if (fseek(fp, 0, SEEK_END) != 0 || ftell(fp) != (long)(len1 + len2 + strlen(extra))) {
        printf("  FAIL: fseek(SEEK_END)/ftell failed\n");
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0 || ftell(fp) != 0) {
        printf("  FAIL: fseek(SEEK_SET)/ftell failed\n");
        fclose(fp);
        return -1;
    }

    char raw[32];
    memset(raw, 0, sizeof(raw));
    size_t nread = fread(raw, 1, 5, fp);
    printf("  fread(5) at offset 0: '%s' (n=%llu)\n", raw, (unsigned long long)nread);

    fclose(fp);

    if (rename(path, renamed) != 0 || !fs_exists(renamed) || fs_exists(path)) {
        printf("  FAIL: rename failed\n");
        return -1;
    }

    if (remove(renamed) != 0) {
        printf("  FAIL: remove failed\n");
        return -1;
    }
    printf("  PASS: file lifecycle OK\n");
    return 0;
}

static int test_heap_and_ipc(void) {
    printf("[2] Testing sbrk, mmap_anon & IPC...\n");

    void *p1 = brk_current();
    void *p2 = sbrk(4096);
    void *p3 = brk_current();
    printf("  brk before=%p, after 4096=%p, now=%p\n", p1, p2, p3);
    if (!p2 || p2 == (void *)-1) {
        printf("  FAIL: sbrk failed\n");
        return -1;
    }

    memset(p2, 0, 4096);
    volatile uint8_t *heap_bytes = (volatile uint8_t *)p2;
    heap_bytes[0] = 0xAA;
    heap_bytes[4095] = 0x55;
    printf("  heap write/read check: 0x%x / 0x%x\n", heap_bytes[0], heap_bytes[4095]);

    void *mapped = mmap_anon(4096, 0x02ULL);
    if (!mapped) {
        printf("  FAIL: mmap_anon failed\n");
        return -1;
    }
    volatile uint8_t *mbytes = (volatile uint8_t *)mapped;
    mbytes[0] = 0x11;
    mbytes[4095] = 0x22;
    printf("  mmap_anon=%p check: 0x%x / 0x%x\n", mapped, mbytes[0], mbytes[4095]);

    const char *msg = "ping-from-user";
    size_t mlen = strlen(msg);
    int64_t sent = ipc_send_str(msg);
    char rx[64];
    memset(rx, 0, sizeof(rx));
    int64_t recvd = ipc_recv(rx, sizeof(rx) - 1);
    printf("  IPC sent=%lld, recvd=%lld, payload='%s'\n", (long long)sent, (long long)recvd, rx);

    if (sent != (int64_t)mlen || recvd != (int64_t)mlen) {
        printf("  FAIL: IPC mismatch\n");
        return -1;
    }
    printf("  PASS: heap, mmap & IPC OK\n");
    return 0;
}

static int test_doge_misc(void) {
    printf("[3] Testing uptime, time string, rand, beep, sleep\n");

    uint64_t up = uptime();
    char tbuf[32];
    memset(tbuf, 0, sizeof(tbuf));
    int64_t tlen = get_time_str(tbuf, sizeof(tbuf));
    printf("  uptime=%llu s, time='%s' (len=%lld)\n", (unsigned long long)up, tbuf, (long long)tlen);

    uint64_t r = 0;
    int rr = rand64(&r);
    uint32_t r10 = rand_range(1, 10);
    printf("  rand64(rc=%d)=0x%llx, rand_range[1..10]=%u\n", rr, (unsigned long long)r, r10);

    beep(880, 80);
    wait(100);
    printf("  PASS: doge helpers OK\n");
    return 0;
}

__attribute__((noreturn, used)) void _start(void) {
    puts("=== Test ===");

    int failures = 0;
    if (test_stdio_files() != 0) failures++;
    if (test_heap_and_ipc() != 0) failures++;
    if (test_doge_misc() != 0) failures++;

    putchar('\n');
    printf("[4] Getchar test (press a key): ");
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    size_t idx = 0;
    while (idx < sizeof(buffer) - 1) {
        int c = getchar();
        if (c == EOF) continue;
        if (c == '\r' || c == '\n') {
            break;
        }
        buffer[idx++] = (char)c;
    }
    buffer[idx] = '\0';
    printf("Got: %s\n", buffer);

    printf("[5] stdin fgets test (type a line and press Enter): ");
    char line_in[64];
    memset(line_in, 0, sizeof(line_in));
    if (fgets(line_in, sizeof(line_in), stdin)) {
        printf("stdin Got: %s", line_in);
    }

    if (failures == 0) {
        printf("\n=== ALL TESTS PASSED ===\n");
    } else {
        printf("\n=== %d TEST(S) FAILED ===\n", failures);
    }

    exit();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}