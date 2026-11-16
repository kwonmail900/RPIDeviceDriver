/* SPDX-License-Identifier: MIT */
/*
 * LED user application for /dev/rpi_ledchar.
 * Menu mode: sudo ./led_app
 * CLI mode : sudo ./led_app on|off|status|blink 5
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LED_DEVICE "/dev/rpi_ledchar"

static int set_led(int on)
{
    const char *cmd = on ? "1\n" : "0\n";
    int fd = open(LED_DEVICE, O_WRONLY);
    ssize_t result;

    if (fd < 0) {
        perror("open");
        return -1;
    }

    result = write(fd, cmd, strlen(cmd));
    if (result < 0)
        perror("write");
    else if ((size_t)result != strlen(cmd))
        fprintf(stderr, "write: incomplete command\n");

    if (close(fd) < 0) {
        perror("close");
        return -1;
    }
    if (result < 0 || (size_t)result != strlen(cmd))
        return -1;

    printf("LED: %s\n", on ? "ON" : "OFF");
    return 0;
}

static int show_led(void)
{
    char buffer[8];
    int fd = open(LED_DEVICE, O_RDONLY);
    ssize_t result;

    if (fd < 0) {
        perror("open");
        return -1;
    }

    result = read(fd, buffer, sizeof(buffer) - 1);
    if (result < 0)
        perror("read");
    else if (result == 0)
        fprintf(stderr, "read: no state received\n");

    if (close(fd) < 0) {
        perror("close");
        return -1;
    }
    if (result <= 0)
        return -1;

    buffer[result] = '\0';
    if (buffer[0] != '0' && buffer[0] != '1') {
        fprintf(stderr, "read: unexpected state: %s\n", buffer);
        return -1;
    }
    printf("LED state: %s\n", buffer[0] == '1' ? "ON" : "OFF");
    return 0;
}

static void delay_ms(long ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    while (nanosleep(&req, &req) < 0 && errno == EINTR) {
        /* Retry after a signal. */
    }
}

static int blink_led(int times)
{
    for (int i = 0; i < times; ++i) {
        if (set_led(1) < 0)
            return -1;
        delay_ms(500);
        if (set_led(0) < 0)
            return -1;
        delay_ms(500);
    }
    return 0;
}

static int run_menu(void)
{
    char line[64];
    char extra;
    int choice;

    for (;;) {
        printf("\n=== LED Control Application ===\n");
        printf("1. LED ON\n2. LED OFF\n3. Read LED state\n");
        printf("4. Blink 5 times\n0. Exit\nSelect: ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            return 0;
        if (sscanf(line, " %d %c", &choice, &extra) != 1) {
            puts("Invalid menu choice.");
            continue;
        }

        switch (choice) {
        case 1: if (set_led(1) < 0) return 1; break;
        case 2: if (set_led(0) < 0) return 1; break;
        case 3: if (show_led() < 0) return 1; break;
        case 4: if (blink_led(5) < 0) return 1; break;
        case 0: return 0;
        default: puts("Invalid menu choice.");
        }
    }
}

static int usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [on|off|status|blink N] (N: 1..20)\n",
            program);
    return 2;
}

int main(int argc, char *argv[])
{
    char *end;
    long times;

    if (argc == 1)
        return run_menu();
    if (argc == 2 && strcmp(argv[1], "on") == 0)
        return set_led(1) < 0;
    if (argc == 2 && strcmp(argv[1], "off") == 0)
        return set_led(0) < 0;
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return show_led() < 0;
    if (argc == 3 && strcmp(argv[1], "blink") == 0) {
        errno = 0;
        times = strtol(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || times < 1 || times > 20)
            return usage(argv[0]);
        return blink_led((int)times) < 0;
    }
    return usage(argv[0]);
}
