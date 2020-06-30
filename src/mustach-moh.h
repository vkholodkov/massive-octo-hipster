//
// Created by gsaggu on 11/16/19.
//

#ifndef _MUSTACHE_MOH_H
#define _MUSTACHE_MOH_H

#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <bstring.h>

#include "grammar.h"
#include "mustach.h"
#include "output.h"
#include "vector.h"


#define MUSTACH_TEMP_TOKEN_SIZE 60
#define MUSTACH_TOKEN_SIGN "||->"

int start(void *closure);

int enter(void *closure, const char *name);

int next(void *closure);

int leave(void *closure);

int get(void *closure, const char *name, struct mustach_sbuf *sbuf);

int emit(void *closure, const char *buffer, size_t size, int escape, FILE *file);

static struct mustach_itf itf = {
        .start      = start,
        .put        = NULL,
        .enter      = enter,
        .next       = next,
        .leave      = leave,
        /* Not using partial for now */
        .partial    = NULL,
        .get        = get,
        .emit       = emit,
        .stop       = NULL
};

typedef struct {
    boo_vector_t *extra_tokens;
    boo_grammar_t *grammar;
    boo_output_t *output;
    boo_str_t filename;
    const char *template;
} closure_t;

typedef struct {
    char            *token;
    boo_vector_t    *data;
} extra_token_t;

enum token_value {
    PREFIX,
    CONTEXT,
    PREFIX_UPPER,
    FILENAME,
    UNKNOWN, // Unknown for extra literals (if any)
    ARRAY_FINISH // We use END for array finish hint
};

typedef struct {
    char *name;
    enum token_value id;
} token_name_t;

int fmustach_moh(closure_t *closure, FILE *file);

boo_int_t read_mustach_file(const char *filename, closure_t *closure);
boo_int_t read_extra_tokens_file(const char *filename, pool_t *pool, closure_t *closure);

#endif //_MUSTACHE_MOH_H
