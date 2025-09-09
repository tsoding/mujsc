# MuJS Compiler

## Quick Start

```console
$ cc -o nob nob.c
$ ./nob
$ cat ./examples/example.js
var a = 34;
var b = 35;
print_int(a);
a += b;
print_int(a);
$ ./build/mujsc ./examples/example.js ./examples/example
$ ./examples/example
34
69
```
