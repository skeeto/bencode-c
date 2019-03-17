#include <stdlib.h>
#include <string.h>
#include "bencode.h"

void
bencode_reinit(struct bencode *ctx, const void *buf, size_t len)
{
    ctx->tok = 0;
    ctx->toklen = 0;
    ctx->buf = buf;
    ctx->buflen = len;
    ctx->size = 0;
}

void
bencode_init(struct bencode *ctx, const void *buf, size_t len)
{
    ctx->tok = 0;
    ctx->toklen = 0;
    ctx->buf = buf;
    ctx->buflen = len;
    ctx->stack = 0;
    ctx->cap = 0;
    ctx->size = 0;
}

void
bencode_free(struct bencode *ctx)
{
    free(ctx->stack);
    ctx->stack = 0;
}

static int
bencode_get(struct bencode *ctx)
{
    const unsigned char *p = ctx->buf;
    if (!ctx->buflen)
        return -1;
    ctx->buflen--;
    ctx->buf = p + 1;
    return *p;
}

static int
bencode_peek(struct bencode *ctx)
{
    if (!ctx->buflen)
        return -1;
    return *(unsigned char *)ctx->buf;;
}

static size_t
bencode_push(struct bencode *ctx)
{
    if (ctx->size == ctx->cap) {
        void *newstack;
        size_t bytes, newcap;
        if (!ctx->stack) {
            newcap = 64;
        } else {
            newcap = ctx->cap * 2;
            if (!newcap) return -1;
        }
        bytes = newcap * sizeof(ctx->stack[0]);
        if (!bytes) return -1;
        newstack = realloc(ctx->stack, bytes);
        if (!newstack) return -1;
        ctx->stack = newstack;
    }
    return ctx->size++;
}

static int
bencode_integer(struct bencode *ctx)
{
    int c;

    ctx->tok = ctx->buf;

    c = bencode_get(ctx);
    switch (c) {
        case -1:
            return BENCODE_ERROR_EOF;
        case 0x2d: /* - */
            c = bencode_get(ctx);
            if (c == -1)
                return BENCODE_ERROR_EOF;
            if (c < 0x31 || c > 0x39) /* 1-9 */
                return BENCODE_ERROR_INVALID;
            break;
        case 0x30: /* 0 */
            c = bencode_get(ctx);
            if (c == -1)
                return BENCODE_ERROR_EOF;
            if (c != 0x65) /* e */
                return BENCODE_ERROR_INVALID;
            ctx->toklen = 1;
            return BENCODE_INTEGER;
    }
    if (c < 0x30 || c > 0x39)
        return BENCODE_ERROR_INVALID;

    /* Read until 'e' */
    do
        c = bencode_get(ctx);
    while (c >= 0x30 && c <= 0x39);
    if (c == -1)
        return BENCODE_ERROR_EOF;
    if (c != 0x65) /* e */
        return BENCODE_ERROR_INVALID;
    ctx->toklen = (char *)ctx->buf - (char *)ctx->tok - 1;
    return BENCODE_INTEGER;
}

static int
bencode_string(struct bencode *ctx)
{
    int c;
    const unsigned char *tok = (unsigned char *)ctx->buf - 1;

    /* Consume the remaining digits */
    do
        c = bencode_get(ctx);
    while (c >= 0x30 && c <= 0x39);
    if (c == -1)
        return BENCODE_ERROR_EOF;
    if (c != 0x3a) /* : */
        return BENCODE_ERROR_INVALID;

    /* Decode the length */
    ctx->tok = ctx->buf;
    ctx->toklen = 0;
    for (; tok < (unsigned char *)ctx->buf - 1; tok++) {
        size_t n = ctx->toklen * 10 + (*tok - 0x30);
        if (n < ctx->toklen) {
            /* Overflow: length definitely extends beyond the buffer size */
            return BENCODE_ERROR_EOF;
        }
        ctx->toklen = n;
    }

    /* Advance input to end of string */
    if (ctx->buflen < ctx->toklen)
        return BENCODE_ERROR_EOF;
    ctx->buf = (char *)ctx->buf + ctx->toklen;
    ctx->buflen -= ctx->toklen;
    return BENCODE_STRING;
}

int
bencode_next(struct bencode *ctx)
{
    int c, r;
    size_t i;
    void **keyptr = 0;
    size_t *keylenptr = 0;

    if (ctx->size) {
        int *flags = &ctx->stack[ctx->size - 1].flags;
        *flags &= ~BENCODE_FLAG_FIRST;
        if (*flags & BENCODE_FLAG_DICT) {
            /* Inside a dictionary, validate it */
            int c = bencode_peek(ctx);
            if (*flags & BENCODE_FLAG_EXPECT_VALUE) {
                /* Cannot end dictionary here */
                if (c == 0x65)
                    return BENCODE_ERROR_INVALID;
                *flags &= ~BENCODE_FLAG_EXPECT_VALUE;
            } else {
                /* Next value must look like a string or 'e' */
                if (c != 0x65 && (c < 0x30 || c > 0x39)) /* e, 0-9 */
                    return BENCODE_ERROR_INVALID;
                *flags |= BENCODE_FLAG_EXPECT_VALUE;
                keyptr = &ctx->stack[ctx->size - 1].key;
                keylenptr = &ctx->stack[ctx->size - 1].keylen;
            }
        }
    } else if (ctx->buflen == 0) {
        return ctx->tok ? BENCODE_DONE : BENCODE_ERROR_EOF;
    }

    r = BENCODE_ERROR_INVALID;
    c = bencode_get(ctx);
    switch (c) {
        case -1:
            return BENCODE_ERROR_EOF;
        case 0x64: /* d */
            i = bencode_push(ctx);
            if (i == (size_t)-1)
                return BENCODE_ERROR_OOM;
            ctx->stack[i].key = 0;
            ctx->stack[i].keylen = 0;
            ctx->stack[i].flags = BENCODE_FLAG_DICT | BENCODE_FLAG_FIRST;
            return BENCODE_DICT_BEGIN;
        case 0x65: /* e */
            if (!ctx->size)
                return BENCODE_ERROR_INVALID;
            i = --ctx->size;
            if (ctx->stack[i].flags & BENCODE_FLAG_DICT)
                return BENCODE_DICT_END;
            return BENCODE_LIST_END;
        case 0x69: /* i */
            return bencode_integer(ctx);
        case 0x6c: /* l */
            i = bencode_push(ctx);
            if (i == (size_t)-1)
                return BENCODE_ERROR_OOM;
            ctx->stack[i].flags = BENCODE_FLAG_FIRST;
            return BENCODE_LIST_BEGIN;
        case 0x30: /* 0 */
            c = bencode_get(ctx);
            if (c == -1)
                return BENCODE_ERROR_EOF;
            if (c != 0x3a) /* : */
                return BENCODE_ERROR_INVALID;
            ctx->tok = ctx->buf;
            ctx->toklen = 0;
            r = BENCODE_STRING;
            break;
        case 0x31: /* 1 */
        case 0x32: /* 2 */
        case 0x33: /* 3 */
        case 0x34: /* 4 */
        case 0x35: /* 5 */
        case 0x36: /* 6 */
        case 0x37: /* 7 */
        case 0x38: /* 8 */
        case 0x39: /* 9 */
            r = bencode_string(ctx);
            break;
    }

    if (r == BENCODE_STRING && keyptr) {
        /* Enforce key ordering */
        if (*keyptr) {
            if (ctx->toklen < *keylenptr) {
                if (memcmp(ctx->tok, *keyptr, ctx->toklen) <= 0)
                    return BENCODE_ERROR_BAD_KEY;
            } else if (*keylenptr < ctx->toklen ) {
                if (memcmp(ctx->tok, *keyptr, *keylenptr) < 0)
                    return BENCODE_ERROR_BAD_KEY;
            } else {
                if (memcmp(ctx->tok, *keyptr, ctx->toklen) <= 0)
                    return BENCODE_ERROR_BAD_KEY;
            }
        }
        *keyptr = (void *)ctx->tok;
        *keylenptr = ctx->toklen;
    }

    return r;
}
