#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "./nob.h"

#define MUJS_FOLDER     "mujs-1.3.7/"
#define BUILD_FOLDER    "build/"
#define EXAMPLES_FOLDER "examples/"
#define MUJSC_EXE        BUILD_FOLDER"mujsc"

Cmd cmd = {0};

bool debug = false;
bool run   = false;

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program_name = shift(argv, argc);

    while (argc > 0) {
        const char *flag = shift(argv, argc);
        if (strcmp(flag, "-debug") == 0) { debug = true; continue; }
        if (strcmp(flag, "-run")   == 0) { run   = true; continue; }
        if (strcmp(flag, "--")     == 0) break;
        fprintf(stderr, "%s:%d: ERROR: unknown flag `%s`\n", __FILE__, __LINE__, flag);
        if (run) {
            fprintf(stderr, "NOTE: use -- to separate %s and %s command line arguments\n", program_name, MUJSC_EXE);
        }
        return 1;
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-I"MUJS_FOLDER);
    cmd_append(&cmd, "-Wall");
    if (debug) {
        cmd_append(&cmd, "-ggdb");
        cmd_append(&cmd, "-DDEBUG");
    }
    cmd_append(&cmd, "-o", MUJSC_EXE);
    cmd_append(&cmd, "main.c");
    cmd_append(&cmd, MUJS_FOLDER"one.c");
    cmd_append(&cmd, "-lm");
    if (!cmd_run(&cmd)) return 1;

    if (run) {
        if (debug) {
            cmd_append(&cmd, "gf2");
        }
        cmd_append(&cmd, MUJSC_EXE);
        cmd_append(&cmd, EXAMPLES_FOLDER"hello.js");
        cmd_append(&cmd, BUILD_FOLDER"hello");
        da_append_many(&cmd, argv, argc);
        if (!cmd_run(&cmd)) return 1;
    }

    return 0;
}
