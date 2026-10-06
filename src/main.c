/*
 * main.c - 터미널 입력/시연
 * 담당: F (이태식)
 *
 *   ./build/vbc              대화형: 명령을 한 줄씩 입력
 *   ./build/vbc <파일>       시나리오: 파일의 명령을 차례로 실행하고, expect가 하나라도
 *                            틀리거나 잘못된 명령이 있으면 종료 코드 1
 * 명령 목록: help 또는 docs/console.md
 */

#include <stdio.h>
#include <string.h>

#include "console.h"

static int run_interactive(void)
{
    Console c;
    char line[256];

    console_init(&c, stdout);
    printf("차체 편의 기능 시뮬레이터 (VBC-SRS-001). help: 명령 목록, quit: 종료\n");
    printf("시간은 wait 명령으로 흐른다. 시작: 시동 OFF, 기어 P, 속도 0, 조도 30000, 강우 0%%\n");
    for (;;) {
        printf("%u.%03us> ", (unsigned)(c.now / 1000u), (unsigned)(c.now % 1000u));
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin))
            break;
        if (console_exec(&c, line) == CONSOLE_QUIT)
            break;
    }
    putchar('\n');
    return 0;
}

static int run_script(const char *path)
{
    Console c;
    char line[256];
    int errors = 0;
    FILE *f = fopen(path, "r");

    if (!f) {
        perror(path);
        return 2;
    }
    console_init(&c, stdout);
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
            printf("> %s\n", line);
        ConsoleResult r = console_exec(&c, line);
        if (r == CONSOLE_QUIT)
            break;
        if (r == CONSOLE_ERROR)
            errors++;
    }
    fclose(f);

    printf("== %s: expect 불일치 %d건, 명령 오류 %d건\n", path, c.expect_failures, errors);
    return (c.expect_failures || errors) ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr, "사용법: %s [시나리오 파일]\n", argv[0]);
        return 2;
    }
    return argc == 2 ? run_script(argv[1]) : run_interactive();
}
