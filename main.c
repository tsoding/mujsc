#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#include <mujs.h>
#include <jsi.h>

Cmd cmd = {0};

void usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s <input.js>\n", program_name);
}

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Strings;

static const char *console_js = "var console = { log: print, debug: print, warn: print, error: print };";

static const char *output_path = NULL;
static void jsB_set_output_path(js_State *J)
{
    output_path = strdup(js_tostring(J, 1));
    js_pushundefined(J);
}

static void jsB_print(js_State *J)
{
    int i, top = js_gettop(J);
    for (i = 1; i < top; ++i) {
        const char *s = js_tostring(J, i);
        if (i > 1) putchar(' ');
        fputs(s, stdout);
    }
    putchar('\n');
    js_pushundefined(J);
}

int main(int argc, char **argv)
{
    const char *program_name = shift(argv, argc);

    if (argc <= 0) {
        usage(program_name);
        fprintf(stderr, "ERROR: no input is provided\n");
        return 1;
    }
    const char *filename = shift(argv, argc);

    {
        String_View filename_sv = sv_from_cstr(filename);
        const char *js_ext = ".js";
        if (sv_end_with(filename_sv, js_ext)) {
            size_t n = strlen(js_ext);
            filename_sv.count -= n;
        }
        output_path = temp_sv_to_cstr(filename_sv);
    }

    js_State *J = js_newstate(NULL, NULL, 0);
    assert(J != NULL);

    js_newcfunction(J, jsB_print, "print", 0);
    js_setglobal(J, "print");

    js_newcfunction(J, jsB_set_output_path, "set_output_path", 0);
    js_setglobal(J, "set_output_path");

    int ret = js_dostring(J, console_js);
    assert(ret == 0);

    String_Builder sb_source = {0};
    if (!read_entire_file(filename, &sb_source)) return 1;
    sb_append_null(&sb_source);

    const char *source = sb_source.items;

    js_Ast *P;
    js_Function *F;

    if (js_try(J)) {
        printf("EXCEPTION: %s\n", js_tostring(J, -1));
        return 69;
    }

    P = jsP_parse(J, filename, source);
    F = jsC_compilescript(J, P, J->default_strict);
    jsP_freeparse(J);

    // pc = ..... [OP_SETVAR] [ ] [ ] [ ] [ ]
    //                                        ^

    js_Function **FT = F->funtab;
    for (int i = 0; i < F->funlen; ++i) {
        if (strcmp(FT[i]->name, "build") == 0) {
            js_Object *obj = jsV_newobject(J, JS_CFUNCTION, J->Function_prototype);
            obj->u.f.function = FT[i];
            obj->u.f.scope = J->E;
            js_pushobject(J, obj);
            js_pushundefined(J);
            js_call(J, 0);
        }
    }

    const char *str;
#define READSTRING() \
    memcpy(&str, pc, sizeof(str)); \
    pc += sizeof(str) / sizeof(*pc)

    //                          rbp
    //                          v
    // ..bbbbaaaarbp*____________
    //               ^rsp

    String_Builder sb_out = {0};
    sb_appendf(&sb_out, ".global main\n");
    sb_appendf(&sb_out, "main:\n");
    sb_appendf(&sb_out, "    pushq %%rbp\n");
    sb_appendf(&sb_out, "    movq %%rsp, %%rbp\n");
    sb_appendf(&sb_out, "    subq $%d, %%rsp\n", 8*(F->varlen + 1));

    js_Instruction *pc = F->code;
    js_Instruction *pc_end = F->code + F->codelen;

    Strings strings = {0};

    while (pc < pc_end) {
        sb_appendf(&sb_out, "op_%ld: ", pc - F->code);

        int line = *pc++;
        enum js_OpCode opcode = *pc++;

        switch (opcode) {
        case OP_POP: {
            sb_appendf(&sb_out, "// OP_POP %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    popq %%rax\n");
        } break;
        case OP_DUP: {
            sb_appendf(&sb_out, "// OP_DUP %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    pushq (%%rsp)\n");
        } break;
        case OP_DUP2:       TODO("OP_DUP2");       break;
        case OP_ROT2: {
            sb_appendf(&sb_out, "// OP_ROT2 %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    popq %%rbx\n");
            sb_appendf(&sb_out, "    pushq %%rax\n");
            sb_appendf(&sb_out, "    pushq %%rbx\n");
        } break;
        case OP_ROT3:       TODO("OP_ROT3");       break;
        case OP_ROT4:       TODO("OP_ROT4");       break;
        case OP_INTEGER: {
            unsigned short lit = *pc++ - 32768;
            sb_appendf(&sb_out, "// OP_INTEGER(%u) %s:%d\n", lit, filename, line);
            sb_appendf(&sb_out, "    pushq $%u\n", lit);
        } break;
        case OP_NUMBER: {
            TODO(temp_sprintf("OP_NUMBER %s:%d", filename, line));
        } break;
        case OP_STRING: {
            READSTRING();
            sb_appendf(&sb_out, "// OP_STRING(%s) %s:%d\n", str, filename, line);
            size_t index = strings.count;
            da_append(&strings, strdup(str));
            sb_appendf(&sb_out, "    pushq $string_%zu\n", index);
        } break;
        case OP_CLOSURE: {
            unsigned short lit = *pc++; // skip the function
            sb_appendf(&sb_out, "// OP_CLOSURE(%u) %s:%d\n", lit, filename, line);
            sb_appendf(&sb_out, "    pushq $0\n"); // TODO: emit function pointer
        } break;
        case OP_NEWARRAY:   TODO("OP_NEWARRAY");   break;
        case OP_NEWOBJECT:  TODO("OP_NEWOBJECT");  break;
        case OP_NEWREGEXP:  TODO("OP_NEWREGEXP");  break;
        case OP_UNDEF: {
            sb_appendf(&sb_out, "// OP_UNDEF %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    pushq $0\n");
        } break;
        case OP_NULL:       TODO("OP_NULL");       break;
        case OP_TRUE:       TODO("OP_TRUE");       break;
        case OP_FALSE:      TODO("OP_FALSE");      break;
        case OP_THIS:       TODO("OP_THIS");       break;
        case OP_CURRENT:    TODO("OP_CURRENT");    break;
        case OP_GETLOCAL: {
            unsigned short index = *pc++;
            sb_appendf(&sb_out, "// OP_GETLOCAL(%u) %s:%d\n", index, filename, line);
            sb_appendf(&sb_out, "    pushq -%u(%%rbp)\n", index*8);
        } break;
        case OP_SETLOCAL: {
            unsigned short index = *pc++;
            sb_appendf(&sb_out, "// OP_SETLOCAL(%u) %s:%d\n", index, filename, line);
            sb_appendf(&sb_out, "    movq (%%rsp), %%rax\n");
            sb_appendf(&sb_out, "    movq %%rax, -%u(%%rbp)\n", index*8);
        } break;
        case OP_DELLOCAL:   TODO("OP_DELLOCAL");   break;
        case OP_HASVAR:     TODO("OP_HASVAR");     break;
        case OP_GETVAR: {
            READSTRING();
            sb_appendf(&sb_out, "// OP_GETVAR(%s) %s:%d\n", str, filename, line);
            sb_appendf(&sb_out, "    pushq $%s\n", str);
        } break;
        case OP_SETVAR: {
            READSTRING();
            TODO(temp_sprintf("OP_SETVAR(%s)", str));
        } break;
        case OP_DELVAR:     TODO("OP_DELVAR");     break;
        case OP_IN:         TODO("OP_IN");         break;
        case OP_SKIPARRAY:  TODO("OP_SKIPARRAY");  break;
        case OP_INITARRAY:  TODO("OP_INITARRAY");  break;
        case OP_INITPROP:   TODO("OP_INITPROP");   break;
        case OP_INITGETTER: TODO("OP_INITGETTER"); break;
        case OP_INITSETTER: TODO("OP_INITSETTER"); break;
        case OP_GETPROP:    TODO("OP_GETPROP");    break;
        case OP_SETPROP:    TODO("OP_SETPROP");    break;
        case OP_DELPROP:    TODO("OP_DELPROP");    break;
        case OP_ITERATOR:   TODO("OP_ITERATOR");   break;
        case OP_NEXTITER:   TODO("OP_NEXTITER");   break;
        case OP_EVAL:       TODO("OP_EVAL");       break;
        case OP_CALL: {
            unsigned short arity = *pc++;

            sb_appendf(&sb_out, "// OP_CALL(%u) %s:%d\n", arity, filename, line);
            const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            assert(arity == 1);
            sb_appendf(&sb_out, "    popq %%%s\n", regs[0]);
            sb_appendf(&sb_out, "    popq %%rax\n"); // skip implicit this
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    callq *%%rax\n");
            sb_appendf(&sb_out, "    pushq %%rax\n");
        } break;
        case OP_NEW:        TODO("OP_NEW");        break;
        case OP_TYPEOF:     TODO("OP_TYPEOF");     break;
        case OP_POS:        TODO("OP_POS");        break;
        case OP_NEG:        TODO("OP_NEG");        break;
        case OP_BITNOT:     TODO("OP_BITNOT");     break;
        case OP_LOGNOT:     TODO("OP_LOGNOT");     break;
        case OP_INC: {
            sb_appendf(&sb_out, "// OP_INC %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    incq (%%rsp)\n");
        } break;
        case OP_DEC:        TODO("OP_DEC");        break;
        case OP_POSTINC:    TODO("OP_POSTINC");    break;
        case OP_POSTDEC:    TODO("OP_POSTDEC");    break;
        case OP_MUL:        TODO("OP_MUL");        break;
        case OP_DIV:        TODO("OP_DIV");        break;
        case OP_MOD:        TODO("OP_MOD");        break;
        case OP_ADD: {
            sb_appendf(&sb_out, "// OP_ADD %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    add %%rax, (%%rsp)\n");
        } break;
        case OP_SUB:        TODO("OP_SUB");        break;
        case OP_SHL:        TODO("OP_SHL");        break;
        case OP_SHR:        TODO("OP_SHR");        break;
        case OP_USHR:       TODO("OP_USHR");       break;
        case OP_LT: {
            // TODO: add some sort of type checking that generates different code depending on the types.
            // Type check similarly to how WebAssembly does the verification step. That is perform a "meta-execution"
            // which pushes not values but their types on the stack.
            sb_appendf(&sb_out, "// OP_LT %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    xorq %%rdx, %%rdx\n");
            sb_appendf(&sb_out, "    popq %%rbx\n");
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    cmpq %%rbx, %%rax\n");
            sb_appendf(&sb_out, "    setl %%dl\n");
            sb_appendf(&sb_out, "    pushq %%rdx\n");
        } break;
        case OP_GT:         TODO("OP_GT");         break;
        case OP_LE:         TODO("OP_LE");         break;
        case OP_GE:         TODO("OP_GE");         break;
        case OP_EQ:         TODO("OP_EQ");         break;
        case OP_NE:         TODO("OP_NE");         break;
        case OP_STRICTEQ:   TODO("OP_STRICTEQ");   break;
        case OP_STRICTNE:   TODO("OP_STRICTNE");   break;
        case OP_JCASE:      TODO("OP_JCASE");      break;
        case OP_BITAND:     TODO("OP_BITAND");     break;
        case OP_BITXOR:     TODO("OP_BITXOR");     break;
        case OP_BITOR:      TODO("OP_BITOR");      break;
        case OP_INSTANCEOF: TODO("OP_INSTANCEOF"); break;
        case OP_THROW:      TODO("OP_THROW");      break;
        case OP_TRY:        TODO("OP_TRY");        break;
        case OP_ENDTRY:     TODO("OP_ENDTRY");     break;
        case OP_CATCH:      TODO("OP_CATCH");      break;
        case OP_ENDCATCH:   TODO("OP_ENDCATCH");   break;
        case OP_WITH:       TODO("OP_WITH");       break;
        case OP_ENDWITH:    TODO("OP_ENDWITH");    break;
        case OP_DEBUGGER:   TODO("OP_DEBUGGER");   break;
        case OP_JUMP: {
            short offset = *pc++;
            sb_appendf(&sb_out, "// OP_JUMP %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    jmp op_%d\n", offset);
        } break;
        case OP_JTRUE:      TODO("OP_JTRUE");      break;
        case OP_JFALSE: {
            short offset = *pc++;
            sb_appendf(&sb_out, "// OP_FALSE %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    testq %%rax, %%rax\n");
            sb_appendf(&sb_out, "    jz op_%d\n", offset);
        } break;
        case OP_RETURN: {
            sb_appendf(&sb_out, "// OP_RETURN %s:%d\n", filename, line);
            sb_appendf(&sb_out, "    movq $0, %%rax\n");
            sb_appendf(&sb_out, "    movq %%rbp, %%rsp\n");
            sb_appendf(&sb_out, "    pop %%rbp\n");
            sb_appendf(&sb_out, "    ret\n");
        } break;
        case OP_GETPROP_S: {
            READSTRING();
            if (strcmp(str, "log") != 0) {
                TODO("Support more properties");
            }
            sb_appendf(&sb_out, "// OP_GETPROP_S(%s) %s:%d\n", str, filename, line);
            sb_appendf(&sb_out, "    popq %%rax\n");
            sb_appendf(&sb_out, "    pushq (%%rax)\n");
        } break;
        case OP_SETPROP_S:  TODO("OP_SETPROP_S");  break;
        case OP_DELPROP_S:  TODO("OP_DELPROP_S");  break;
        default:
            UNREACHABLE(temp_sprintf("UNKNOWN(%u)", opcode));
        }
    }
    js_endtry(J);

    for (size_t i = 0; i < strings.count; ++i) {
        sb_appendf(&sb_out, "string_%zu: .byte ", i);
        size_t n = strlen(strings.items[i]);
        for (size_t j = 0; j < n; ++j) {
            if (j > 0) sb_appendf(&sb_out, ",");
            sb_appendf(&sb_out, "0x%02X", strings.items[i][j]);
        }
        if (n > 0) sb_appendf(&sb_out, ",");
        sb_appendf(&sb_out, "0x00\n");
    }

    const char *output_asm_path = temp_sprintf("%s.asm", output_path);
    if (!write_entire_file(output_asm_path, sb_out.items, sb_out.count)) return 1;
    nob_log(INFO, "Generate %s", output_asm_path);

    const char *output_o_path = temp_sprintf("%s.o", output_path);
    cmd_append(&cmd, "as");
    cmd_append(&cmd, "-o", output_o_path);
    cmd_append(&cmd, "-g");
    cmd_append(&cmd, output_asm_path);
    if (!cmd_run(&cmd)) return 1;

    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-g");
    cmd_append(&cmd, "-no-pie");
    cmd_append(&cmd, "-o", output_path);
    cmd_append(&cmd, output_o_path);
    cmd_append(&cmd, "mujsc_runtime.c");
    if (!cmd_run(&cmd)) return 1;

    return 0;
}
