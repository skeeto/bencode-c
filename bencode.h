/* Bencode decoder in ANSI C
 *
 * This library only allocates a small stack, though it expects the
 * entire input at once up front. All returned pointers point into this
 * user-supplied buffer.
 *
 * This is free and unencumbered software released into the public domain.
 */
#ifndef BENCODE_H
#define BENCODE_H

#include <stddef.h>

#define BENCODE_ERROR_OOM        -4
#define BENCODE_ERROR_BAD_KEY    -3
#define BENCODE_ERROR_EOF        -2
#define BENCODE_ERROR_INVALID    -1
#define BENCODE_DONE              0
#define BENCODE_INTEGER           1
#define BENCODE_STRING            2
#define BENCODE_LIST_BEGIN        3
#define BENCODE_LIST_END          4
#define BENCODE_DICT_BEGIN        5
#define BENCODE_DICT_END          6

#define BENCODE_FLAG_FIRST         (1 << 0)
#define BENCODE_FLAG_DICT          (1 << 1)
#define BENCODE_FLAG_EXPECT_VALUE  (1 << 2)

/**
 * Return 1 if next element will be the first element at this nesting.
 * This is a helper macro.
 */
#define BENCODE_FIRST(ctx) \
    ((ctx)->size ? (ctx)->stack[(ctx)->size - 1].flags & BENCODE_FLAG_FIRST \
                 : !ctx->tok)
/**
 * Return 1 if next element is a dictionary value.
 * This is a helper macro.
 */
#define BENCODE_IS_VALUE(ctx) \
    ((ctx)->size && \
     ((ctx)->stack[(ctx)->size - 1].flags & BENCODE_FLAG_DICT) && \
     ((ctx)->stack[(ctx)->size - 1].flags & BENCODE_FLAG_EXPECT_VALUE))

struct bencode {
    const void *tok;
    size_t toklen;
    const void *buf;
    size_t buflen;
    struct {
        void *key;
        size_t keylen;
        int flags;
    } *stack;
    size_t cap;
    size_t size;
};

/**
 * Initialize a new decoder on the given buffer.
 *
 * This function cannot fail.
 */
void bencode_init(struct bencode *, const void *, size_t);

/**
 * Start parsing a fresh data buffer.
 *
 * Use this on an encoder previously initalized with bencode_init(), but
 * never freed with bencode_free(). This will reuse memory allocated for
 * the previous parsing tasks.
 */
void bencode_reinit(struct bencode *, const void *, size_t);

/**
 * Destroy the given encoder by freeing any resources.
 */
void bencode_free(struct bencode *);

/**
 * Return the next token in the input stream.
 *
 * Non-negative return values indicate success, negative return values
 * indicate errors. Errors are not recoverable, including OOM, though
 * bencode_free() and bencode_reset() will still work correctly.
 *
 * Returns one of the following values:
 *
 * BENCODE_DONE: Input parsed to completion without errors, including a
 * check for trailing garbage.
 *
 * BENCODE_INTEGER: Found an integer whose text representation is found
 * in the "tok" and "toklen" members of the parser object. This is
 * guaranteed to contain a valid integer, but you must parse the integer
 * yourself.
 *
 * BENCODE_STRING: Found a string, whose content can be found in the
 * "tok" and "toklen" members of the parser object.
 *
 * BENCODE_LIST_BEGIN: Found the beginning of a list.
 *
 * BENCODE_LIST_END: Found the end of the current list. This will always
 * be correctly paired with a BENCODE_LIST_BEGIN.
 *
 * BENCODE_DICT_BEGIN: Found the beginning of a dictionary. While inside
 * the dictionary, the parser will alternate between a string (key) and
 * another object (value).
 *
 * BENCODE_DICT_END: Found the end of the current dictionary. This will
 * always be correctly paired with a BENCODE_DICT_BEGIN.
 *
 * BENCODE_ERROR_INVALID: Found an invalid byte in the input. The "buf"
 * member of the parser object will point at the invalid byte.
 *
 * BENCODE_ERROR_EOF: The input was exhausted early, indicating
 * truncated input.
 *
 * BENCODE_ERROR_BAD_KEY: An invalid key was found while parsing a
 * dictionary. The key is either a duplicate or not properly sorted. The
 * offending key can be found in the "tok" and "toklen" members.
 *
 * BENCODE_ERROR_OOM: The input was so deeply nested that the parser ran
 * of memory for the stack.
 */
int bencode_next(struct bencode *);

#endif
