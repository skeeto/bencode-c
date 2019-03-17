#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../bencode.h"

#if _WIN32
#  define C_RED(s)   s
#  define C_GREEN(s) s
#  define C_BOLD(s)  s
#else
#  define C_RED(s)   "\033[31;1m" s "\033[0m"
#  define C_GREEN(s) "\033[32;1m" s "\033[0m"
#  define C_BOLD(s)  "\033[1m"    s "\033[0m"
#endif

struct expect {
    int type;
    const char *str;
};

#define countof(a) (sizeof(a) / sizeof(*a))

#define TEST(name) \
    do { \
        int r = test(name, seq, countof(seq), str, sizeof(str) - 1); \
        if (r) \
            count_pass++; \
        else \
            count_fail++; \
    } while (0)

const char *
typename(int t)
{
    static const char *const table[] = {
        "ERROR_OOM",
        "ERROR_BAD_KEY",
        "ERROR_EOF",
        "ERROR_INVALID",
        "DONE",
        "INTEGER",
        "STRING",
        "LIST_BEGIN",
        "LIST_END",
        "DICT_BEGIN",
        "DICT_END"
    };
    return table[t + 4];
}

static int
has_value(int type)
{
    return type == BENCODE_INTEGER || type == BENCODE_STRING;
}

static int
test(const char *name,
     struct expect *seq,
     size_t seqlen,
     const char *buf,
     size_t len)
{
    size_t i;
    int success = 1;
    struct bencode ctx[1];
    int expect, actual;
    size_t expect_len, actual_len;
    const char *expect_str, *actual_str;

    bencode_init(ctx, buf, len);
    for (i = 0; success && i < seqlen; i++) {
        expect = seq[i].type;
        actual = bencode_next(ctx);
        actual_str = has_value(actual) ? ctx->tok : 0;
        actual_len = has_value(actual) ? ctx->toklen : 0;
        expect_str = seq[i].str ? seq[i].str : "";
        expect_len = seq[i].str ? strlen(expect_str) : 0;

        if (actual != expect) {
            success = 0;
        } else if (seq[i].str) {
            if (expect_len != actual_len)
                success = 0;
            else if (memcmp(expect_str, actual_str, expect_len))
                success = 0;
        }
    }

    if (success) {
        printf(C_GREEN("PASS") " %s\n", name);
    } else {
        printf(C_RED("FAIL") " %s: "
               "expect " C_BOLD("%s") " %s / "
               "actual " C_BOLD("%s") " %.*s\n",
               name,
               typename(expect), expect_str,
               typename(actual), (int)actual_len, actual_str);
    }
    bencode_free(ctx);
    return success;
}

int
main(void)
{
    int count_pass = 0;
    int count_fail = 0;

    /* Minimal validation checks */

    {
        const char str[] = "";
        struct expect seq[] = {
            {BENCODE_ERROR_EOF}
        };
        TEST("empty");
    }

    {
        const char str[] = "i0e ";
        struct expect seq[] = {
            {BENCODE_INTEGER, "0"},
            {BENCODE_ERROR_INVALID}
        };
        TEST("trailing garbage");
    }

    {
        const char str[] = " i0e";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID}
        };
        TEST("leading garbage");
    }

    /* Integer tests */

    {
        const char str[] = "i0e";
        struct expect seq[] = {
            {BENCODE_INTEGER, "0"},
            {BENCODE_DONE},
        };
        TEST("zero");
    }

    {
        const char str[] = "i-1e";
        struct expect seq[] = {
            {BENCODE_INTEGER, "-1"},
            {BENCODE_DONE},
        };
        TEST("negative");
    }

    {
        const char str[] = "i1e";
        struct expect seq[] = {
            {BENCODE_INTEGER, "1"},
            {BENCODE_DONE},
        };
        TEST("positive");
    }

    {
        const char str[] = "i1234567e";
        struct expect seq[] = {
            {BENCODE_INTEGER, "1234567"},
            {BENCODE_DONE},
        };
        TEST("long positive");
    }

    {
        const char str[] = "i-1234567e";
        struct expect seq[] = {
            {BENCODE_INTEGER, "-1234567"},
            {BENCODE_DONE},
        };
        TEST("long negative");
    }

    {
        const char str[] = "i01e";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("leading zero");
    }

    {
        const char str[] = "i-01e";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("leading zero negative");
    }

    {
        const char str[] = "i-e";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("empty negative");
    }

    {
        const char str[] = "iae";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("garbage integer");
    }

    {
        const char str[] = "i1ae";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("garbage integer 2");
    }

    {
        const char str[] = "ie";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID},
        };
        TEST("empty integer");
    }

    {
        const char str[] = "i0";
        struct expect seq[] = {
            {BENCODE_ERROR_EOF},
        };
        TEST("missing integer terminator");
    }

    /* String tests */

    {
        const char str[] = "5:hello";
        struct expect seq[] = {
            {BENCODE_STRING, "hello"},
        };
        TEST("string");
    }

    {
        const char str[] = "0:";
        struct expect seq[] = {
            {BENCODE_STRING, ""},
        };
        TEST("empty string");
    }

    {
        const char str[] = "01:x";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID}
        };
        TEST("leading zero string");
    }

    {
        const char str[] = "2:x";
        struct expect seq[] = {
            {BENCODE_ERROR_EOF}
        };
        TEST("truncated string");
    }

    {
        const char str[] = "1000000000000000000000000000000000000000:x";
        struct expect seq[] = {
            {BENCODE_ERROR_EOF}
        };
        TEST("ridiculous string");
    }

    {
        const char str[] = "5hello";
        struct expect seq[] = {
            {BENCODE_ERROR_INVALID}
        };
        TEST("missing colon");
    }

    /* List tests */

    {
        const char str[] = "le";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_LIST_END}
        };
        TEST("empty list");
    }

    {
        const char str[] = "li0ee";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_INTEGER, "0"},
            {BENCODE_LIST_END}
        };
        TEST("single element list");
    }

    {
        const char str[] = "li0e5:helloe";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_INTEGER, "0"},
            {BENCODE_STRING, "hello"},
            {BENCODE_LIST_END}
        };
        TEST("double element list");
    }

    {
        const char str[] = "li1el5:helloleee";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_INTEGER, "1"},
            {BENCODE_LIST_BEGIN},
            {BENCODE_STRING, "hello"},
            {BENCODE_LIST_BEGIN},
            {BENCODE_LIST_END},
            {BENCODE_LIST_END},
            {BENCODE_LIST_END}
        };
        TEST("nested list");
    }

    {
        const char str[] = "l";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_ERROR_EOF}
        };
        TEST("truncated list");
    }

    {
        const char str[] = "li0e";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_INTEGER, "0"},
            {BENCODE_ERROR_EOF}
        };
        TEST("truncated list 2");
    }

    {
        const char str[] = "lee";
        struct expect seq[] = {
            {BENCODE_LIST_BEGIN},
            {BENCODE_LIST_END},
            {BENCODE_ERROR_INVALID}
        };
        TEST("list extra terminator");
    }

    /* Dictionary tests */

    {
        const char str[] = "de";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_DICT_END}
        };
        TEST("empty dictionary");
    }

    {
        const char str[] = "d5:helloi42ee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "hello"},
            {BENCODE_INTEGER, "42"},
            {BENCODE_DICT_END}
        };
        TEST("simple dictionary");
    }

    {
        const char str[] = "d5:helloi42e4:xxxx0:e";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "hello"},
            {BENCODE_INTEGER, "42"},
            {BENCODE_STRING, "xxxx"},
            {BENCODE_STRING, ""},
            {BENCODE_DICT_END}
        };
        TEST("simple dictionary 2");
    }

    {
        const char str[] = "d5:hellod3:aaai0ee1:xi-1ee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "hello"},
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "aaa"},
            {BENCODE_INTEGER, "0"},
            {BENCODE_DICT_END},
            {BENCODE_STRING, "x"},
            {BENCODE_INTEGER, "-1"},
            {BENCODE_DICT_END}
        };
        TEST("nested dictionary");
    }

    {
        const char str[] = "dee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_DICT_END},
            {BENCODE_ERROR_INVALID}
        };
        TEST("dictionary extra terminator");
    }

    {
        const char str[] = "di1ei1ee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_ERROR_INVALID}
        };
        TEST("dictionary integer key");
    }

    {
        const char str[] = "d1:bi0e1:ai0ee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "b"},
            {BENCODE_INTEGER, "0"},
            {BENCODE_ERROR_BAD_KEY}
        };
        TEST("wrong key order");
    }

    {
        const char str[] = "d1:dd1:a1:11:b1:2e1:ci0ee";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "d"},
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "a"},
            {BENCODE_STRING, "1"},
            {BENCODE_STRING, "b"},
            {BENCODE_STRING, "2"},
            {BENCODE_DICT_END},
            {BENCODE_ERROR_BAD_KEY}
        };
        TEST("nested wrong key order");
    }

    {
        const char str[] = "d1:a1:11:a1:2e";
        struct expect seq[] = {
            {BENCODE_DICT_BEGIN},
            {BENCODE_STRING, "a"},
            {BENCODE_STRING, "1"},
            {BENCODE_ERROR_BAD_KEY}
        };
        TEST("duplicate key");
    }

    printf("%d pass, %d fail\n", count_pass, count_fail);
    return count_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
