
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include "boo.h"
#include "grammar.h"
#include "string.h"
#include "pool.h"

boo_int_t bootstrap_parse_file(boo_grammar_t *grammar, pool_t *pool, boo_str_t *filename);

boo_str_t target_filename = boo_string("a.out");

boo_int_t build_project(char **filenames, boo_uint_t num_filenames) {
    boo_int_t result, i;
    pool_t *p, *tmp_pool;
    boo_str_t *_filenames;
    boo_grammar_t *grammar;

    p = pool_create();

    if(p == NULL) {
        return BOO_ERROR;
    }

    tmp_pool = pool_create();

    if(tmp_pool == NULL) {
        result = BOO_ERROR;
        goto cleanup;
    }

    _filenames = palloc(tmp_pool, sizeof(boo_str_t*)*num_filenames);

    if(filenames == NULL) {
        result = BOO_ERROR;
        goto cleanup;
    }

    for(i = 0;i < num_filenames;i++) {
        _filenames[i].data = (u_char*)filenames[i];
        _filenames[i].len = strlen(filenames[i]);
    }

    grammar = grammar_create(p);

    if(grammar == NULL) {
        result = BOO_ERROR;
        goto cleanup1;
    }

    result = bootstrap_parse_file(grammar, tmp_pool, &_filenames[0]);

    if(result != BOO_OK) {
        result = BOO_ERROR;
        goto cleanup1;
    }

    result = grammar_generate_lr_item_sets(grammar);

    if(result != BOO_OK) {
        result = BOO_ERROR;
        goto cleanup1;
    }

    grammar_dump_item_sets(&grammar->item_sets);

cleanup1:
    pool_destroy(tmp_pool);

cleanup:
    pool_destroy(p);

    return result;
}

static struct option long_options[] = {
    {"debug", 0, 0, 'd'},
    {0, 0, 0, 0}
};

int main(int argc, char *argv[]) {
    int                 c;
    int                 this_option_optind;
    int                 option_index;

    pool_init();

    while(1) {
        this_option_optind = optind ? optind : 1;
        option_index = 0;

        c = getopt_long(argc, argv, "b:do:v", long_options, &option_index);

        if(c == -1) {
             break;
        }

        switch(c) {
            case 0:
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'b':
#if 0
                result = platform_set_platform(optarg);

                if(result != BOO_OK) {
                    fprintf(stderr, "No support for platform %s is found\n", optarg);
                    return 255;
                }
#endif
                break;

            case 'd':
#if 0
                boodebug = 1;
#endif
                break;

            case 'o':
                target_filename.data = (u_char*)optarg;
                target_filename.len = strlen(optarg);
                break;

            case 'v':
#if 0
                show_commands = 1;
#endif
                break;

            case '?':
                break;
        }
    }

    if(optind < argc) {
        if(build_project(argv + optind, argc - optind) != BOO_OK)
            return 255;
    }
    else {
        fprintf(stderr, "No input files.\n");
    }

    return 0;
}
