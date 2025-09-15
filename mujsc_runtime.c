// This is the file that is automatically linked with a native mujs program compiled with mujsc
#include <stdio.h>

void print_int(int x)
{
    printf("%d\n", x);
}

struct {
    // TODO: console.log should support more types
    void (*log)(int x);
} console = {
    .log = print_int,
};
