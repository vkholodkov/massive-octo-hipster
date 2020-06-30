//
// Created by gsaggu on 11/16/19.
//

#include <stdio.h>
#include <string.h>

#include "mustach-moh.h"

static const size_t BLOCKSIZE = 8192;

char *concat_string_vector(boo_vector_t *vector, pool_t *pool);

const token_name_t token_names[] = {
        {"prefix",       PREFIX},
        {"context",      CONTEXT},
        {"prefix_upper", PREFIX_UPPER},
        {"filename",     FILENAME},

        /* Indicates end of array */
        {"",             ARRAY_FINISH},

        /* For extra tokens */
        {"",             UNKNOWN},
};

int fmustach_moh(closure_t *closure, FILE *file) {
    int rc;
    size_t *size = NULL;
    char *result = NULL;

    if (file != NULL)
        return fmustach(closure->template, &itf, closure, file);

    /* If file pointer is NULL, replace old template with output of the function */
    rc = mustach(closure->template, &itf, closure, &result, size);

    if (rc != MUSTACH_OK)
        goto done;

    free((char *)closure->template);

    closure->template = result;

    done:
    return rc;

}

int start(void *closure) {
    /* Initialize */
    /* Nothing to do for now */
#ifdef debug
    printf("(START)");
#endif
    return MUSTACH_OK;
}

int enter(void *closure, const char *name) {
    /* Enter nested object. Depth += 1 */
    /* Nothing to do for now */
    /* Return 0 for `not entered` */
#ifdef debug
    printf("(ENTER -> %s)", name);
#endif
    return 0;
}

int next(void *closure) {
    /* Get to next section in nested object */
    /* Do nothing for now */
#ifdef debug
    printf("(NEXT)");
#endif
    return 0;
}

int leave(void *closure) {
    /* Leave nested object. Depth -= 1 */
    /* Do nothing for now */
#ifdef debug
    printf("(LEAVE)");
#endif
    return MUSTACH_OK;
}

int get(void *closure, const char *name, struct mustach_sbuf *sbuf) {
#ifdef debug
    printf("(GET -> %s)", name);
#endif
    int i, j;
    enum token_value id;
    token_name_t *found;
    closure_t *boo_output = closure;

    for (i = 0, found = NULL; token_names[i].id != ARRAY_FINISH; i++) {
        if (!strcmp(name, token_names[i].name)) {
            found = (token_name_t *)token_names + i;
            break;
        }
    }

    id = found ? found->id : UNKNOWN;

    switch (id) {
        case PREFIX: {
            sbuf->value = (const char *) boo_output->grammar->prefix->data;
            break;
        }
        case PREFIX_UPPER: {
            sbuf->value = (const char *) boo_output->grammar->prefix_upper->data;
            break;
        }
        case CONTEXT: {
            sbuf->value = (const char *) boo_output->grammar->context->data;
            break;
        }
        case FILENAME: {
            sbuf->value = (const char *) boo_output->filename.data;
            break;
        }
            /* UNKNOWN Case for extra tokens */
            //TODO: Implement hash index if too many tokens.
            // Currently just comparing entire array.
        default: {
            boo_int_t not_found = 1;
            extra_token_t **extra_tokens;

            extra_tokens = boo_output->extra_tokens->elements;
            for (i = 0; i < boo_output->extra_tokens->nelements; i++) {
                if (!strcmp(name, extra_tokens[i]->token)) {
                    not_found = 0;
                    sbuf->value = concat_string_vector(extra_tokens[i]->data,
                                                       boo_output->grammar->pool);
                    break;
                }
            }
            if (not_found) {
                fprintf(stdout, "Unknown token: %s\n", name);
                return MUSTACH_ERROR_ITEM_NOT_FOUND;
            }
            break;
        }
    }

    return MUSTACH_OK;
}

int emit(void *closure, const char *buffer, size_t size, int escape, FILE *file) {
    return fwrite(buffer, size, 1, file) != 1 ? MUSTACH_ERROR_SYSTEM : MUSTACH_OK;
}

//TODO: Copy from mustach-tool.c. Needs to be re-implemented to use pool.
boo_int_t read_mustach_file(const char *filename, closure_t *closure) {
    int f;
    struct stat s;
    char *result;
    size_t size, pos;
    ssize_t rc;

    result = NULL;
    if (filename[0] == '-' && filename[1] == 0)
        f = dup(0);
    else
        f = open((char *) filename, O_RDONLY);
    if (f < 0) {
        fprintf(stderr, "Can't open file: %s\n", filename);
        return BOO_ERROR;
    }

    fstat(f, &s);
    switch (s.st_mode & S_IFMT) {
        case S_IFREG:
            size = s.st_size;
            break;
        case S_IFSOCK:
        case S_IFIFO:
            size = BLOCKSIZE;
            break;
        default:
            fprintf(stderr, "Bad file: %s\n", filename);
            return BOO_ERROR;
    }

    pos = 0;
    result = malloc(size + 1);
    do {
        if (result == NULL) {
            fprintf(stderr, "Out of memory\n");
            return BOO_ERROR;
        }
        rc = read(f, &result[pos], (size - pos) + 1);
        if (rc < 0) {
            fprintf(stderr, "Error while reading %s\n", filename);
            return BOO_ERROR;
        }
        if (rc > 0) {
            pos += (size_t) rc;
            if (pos > size) {
                size = pos + BLOCKSIZE;
                result = realloc(result, size + 1);
            }
        }
    } while (rc > 0);

    close(f);
    result[pos] = 0;
    closure->template = result;

    return BOO_OK;
}

boo_int_t read_extra_tokens_file(const char *filename, pool_t *pool, closure_t *closure) {
    FILE *fin;
    char *line, **lines;
    boo_vector_t *extra_tokens_vector;
    const char temp_token[MUSTACH_TEMP_TOKEN_SIZE];
    extra_token_t *extra_token, **extra_tokens;
    size_t buf = 0;

    extra_tokens_vector = vector_create(pool, sizeof(extra_token_t *), 3);

    if (extra_tokens_vector == NULL) {
        fprintf(stderr, "insufficient memory\n");
        return BOO_ERROR;
    }

    fin = fopen(filename, "r");

    if (fin == NULL) {
        fprintf(stderr, "cannot open input file %s\n", filename);
        return BOO_ERROR;
    }

    while (getline(&line, &buf, fin) != -1) {
        if (sscanf(line, MUSTACH_TOKEN_SIGN"%s", (char *) temp_token)) {
            extra_token = pcalloc(pool, sizeof(extra_token_t));
            extra_token->token = palloc(pool, strlen(temp_token) + 1);
            strcpy(extra_token->token, temp_token);
            memset((void *) temp_token, 0, MUSTACH_TEMP_TOKEN_SIZE);

            extra_tokens = vector_append(extra_tokens_vector);

            if (extra_tokens == NULL) {
                fprintf(stderr, "insufficient memory\n");
                goto cleanup;
            }

            *extra_tokens = extra_token;

        } else {
            if (extra_token == NULL) continue;
            if (extra_token->data == NULL) {
                extra_token->data = vector_create(pool, sizeof(char *), 3);
                if (extra_token->data == NULL) {
                    fprintf(stderr, "insufficient memory\n");
                    goto cleanup;
                }
            }
            lines = vector_append(extra_token->data);

            if (lines == NULL) {
                fprintf(stderr, "insufficient memory\n");
                goto cleanup;
            }

            *lines = palloc(pool, strlen(line) + 1);
            strcpy(*lines, line);
        }
    }

    closure->extra_tokens = extra_tokens_vector;
    fclose(fin);
    return BOO_OK;

    cleanup:
    fclose(fin);
    return BOO_ERROR;

}

char *concat_string_vector(boo_vector_t *vector, pool_t *pool) {
    char *str = NULL, **strings = vector->elements;
    size_t total_length = 0;
    size_t length = 0;
    int i = 0;

    /* Find total length of joined strings */
    for (i = 0; i < vector->nelements; i++) {
        total_length += strlen(strings[i]);
        if (strings[i][strlen(strings[i]) - 1] != '\n')
            ++total_length;
    }
    ++total_length;

    str = (char *) palloc(pool, total_length);
    str[0] = '\0';

    /* Append all the strings */
    for (i = 0; i < vector->nelements; i++) {
        strcat(str, strings[i]);
        length = strlen(str);

        /* Check if we need to insert newline */
        if (str[length - 1] != '\n') {
            str[length] = '\n';
            str[length + 1] = '\0';
        }
    }
    return str;
}