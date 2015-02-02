
#include "lookup.h"

#define boo_trans_len (2 * sizeof(boo_int_t))

boo_lookup_table_t *lookup_create(pool_t *pool, boo_uint_t num_states)
{
    boo_lookup_table_t *lookup;

    lookup = pcalloc(pool, sizeof(boo_lookup_table_t));

    if(lookup == NULL) {
        return NULL;
    }

    lookup->pool = pool;

    lookup->states = pcalloc(pool, num_states * sizeof(boo_lookup_state_t));

    if(lookup->states == NULL) {
        return NULL;
    }

    lookup->num_states = num_states;
    lookup->row_stride = 8;

    return lookup;
}

static boo_gap_t *lookup_alloc_gap(boo_lookup_table_t *lookup) {
    boo_gap_t *g;

    if(lookup->free_gaps != NULL) {
        g = lookup->free_gaps;
        lookup->free_gaps = g->next;
        g->next = NULL;
        return g;
    }

    g = pcalloc(lookup->pool, sizeof(boo_gap_t));

    if(g == NULL) {
        return NULL;
    }

    return g;
}

static void lookup_free_gap(boo_lookup_table_t *lookup, boo_gap_t *g) {
    g->from = g->to = 0;
    g->next = lookup->free_gaps;
    lookup->free_gaps = g;
}

static void lookup_add_gap(boo_lookup_table_t *lookup, boo_gap_t *g)
{
    boo_gap_t *after, *before;

    if(lookup->gaps != NULL) {
        after = NULL;
        before = lookup->gaps;

        while(before != NULL) {
            if(before->from >= g->to) {
                break;
            }
            after = before;
            before = before->next;
        }

        if(after == NULL) {
            g->next = before;
            lookup->gaps = g;
        }
        else {
            g->next = before;
            after->next = g;
        }
    }
    else {
        g->next = NULL;
        lookup->gaps = g;
    }

    fprintf(lookup->debug, "add gap <%u, %u>\n", g->from, g->to);
}

static boo_int_t
lookup_fill_gap(boo_lookup_table_t *lookup, boo_uint_t from, boo_uint_t to) {
    boo_gap_t *p, *next, *prev;
    boo_uint_t p_from, p_to;
    boo_gap_t *new_gaps = NULL, *new_gap;

    fprintf(lookup->debug, "fill_gap <%u, %u>\n", from, to);

    p = lookup->gaps;
    prev = NULL;

    while(p != NULL) {
        next = p->next;

        if((p->from < to) && (p->to > from)) {
            p_from = p->from >= from ? p->from : from;
            p_to = p->to <= to ? p->to : to;

            if(p->from == p_from && p->to == p_to) {
                p->from = p->to;
            }
            else if(p->from == from) {
                p->from = p_to;
            }
            else if(p->to == to) {
                p->to = p_from;
            }
            else {
                new_gap = lookup_alloc_gap(lookup);

                if(new_gap == NULL) {
                    return BOO_ERROR;
                }

                new_gap->from = p_to;
                new_gap->to = p->to;
                new_gap->next = new_gaps;
                new_gaps = new_gap;

                p->to = p_from;
            }

            if(p->from == p->to) {
                if(p == lookup->gaps) {
                    lookup->gaps = next;
                }
                else {
                    prev->next = p->next;
                }
                lookup_free_gap(lookup, p);
            }
        }

        prev = p;

        p = next;
    }

    p = new_gaps;

    while(p != NULL) {
        next = p->next;
        lookup_add_gap(lookup, p);
        p = next;
    }

    return BOO_OK;
}

boo_int_t lookup_add_transition(boo_lookup_table_t *lookup, boo_uint_t state, boo_uint_t symbol, boo_int_t target)
{
    boo_lookup_transition_t *nt, *before, *after;

    nt = pcalloc(lookup->pool, sizeof(boo_lookup_transition_t));

    if(nt == NULL) {
        return BOO_ERROR;
    }

    nt->input = symbol;
    nt->target = target;

    if(lookup->states[state].transitions != NULL) {
        after = NULL;
        before = lookup->states[state].transitions;

        while(before != NULL) {
            if(before->input == nt->input) {
                if(before->target == nt->target) {
                    return BOO_OK;
                }

                return BOO_DECLINED;
            }

            if(before->input > nt->input) {
                break;
            }
            after = before;
            before = before->next;
        }

        if(after == NULL) {
            nt->next = before;
            lookup->states[state].transitions = nt;
        }
        else {
            nt->next = before;
            after->next = nt;
        }
    }
    else {
        nt->next = NULL;
        lookup->states[state].transitions = nt;
    }

    return BOO_OK;
}

boo_int_t lookup_get_transition(boo_lookup_table_t *lookup, boo_uint_t state, boo_uint_t symbol)
{
    boo_lookup_transition_t *t;

    t = lookup->states[state].transitions;

    while(t != NULL) {
        if(t->input == symbol) {
            return t->target;
        }

        t = t->next;
    }

    return BOO_DECLINED;
}

/*
 * Returns BOO_OK if it is possible to fit specified transition table at specified base,
 * Returns BOO_DECLINED otherwise
 */
static boo_int_t
lookup_can_store_transitions(boo_lookup_table_t *lookup, boo_int_t base, boo_lookup_state_t *n)
{
    boo_lookup_transition_t *t;
    boo_uint_t from, to;
    boo_gap_t *g;

    t = n->transitions;
    g = lookup->gaps;

    while(t != NULL) {
        from = base + t->input;

        if(from >= lookup->end) {
            return BOO_OK;
        }

        to = from + 1;

        if(g == NULL) {
            fprintf(stderr, "fatal error: no more gaps, but there are more symbols\n");
            return BOO_DECLINED;
        }

        if((g->from < to) && (g->to > from)) {
            t = t->next;

            if(g->to == to) {
                g = g->next;
            }
        }
        else {
            if(g->from < from) {
                if(g->to >= from) {
                    return BOO_DECLINED;
                }
                g = g->next;
            }
            else {
                return BOO_DECLINED;
            }
        }
    }

    return BOO_OK;
}

static boo_int_t
lookup_book_space(boo_lookup_table_t *lookup, boo_int_t base, boo_lookup_state_t *n) {
    boo_lookup_transition_t *t;
    boo_uint_t pos;
    boo_gap_t *new_gap;
    boo_int_t rc;

    for(t = n->transitions ; t != NULL ; t = t->next) {
        pos = base + t->input;

        if(pos >= lookup->end) {
            if(pos != lookup->end) {
                new_gap = lookup_alloc_gap(lookup);

                if(new_gap == NULL) {
                    return BOO_ERROR;
                }

                new_gap->from = lookup->end;
                new_gap->to = pos;
                new_gap->next = NULL;

                lookup_add_gap(lookup, new_gap);
            }

            lookup->end = pos + 1;

            fprintf(lookup->debug, "extending space to %i, end is now %i\n", pos, lookup->end);
        }

        fprintf(lookup->debug, "placing symbol %u of state %ld pointing to %d, pos %u\n", t->input, n - lookup->states, t->target, pos);

        rc = lookup_fill_gap(lookup, pos, pos + 1);

        if(rc != BOO_OK) {
            return rc;
        }
    }

    return BOO_OK;
}

static boo_int_t
lookup_write_base(FILE *file, boo_lookup_table_t *lookup, const char *prefix)
{
    boo_uint_t i;

    fprintf(file, "static const short %s_base[] = {\n", prefix);

    for(i = 0 ; i != lookup->num_states ; i++) {
        fprintf(file, "%6d,", lookup->states[i].base);

        if(i % lookup->row_stride == lookup->row_stride - 1) {
            fprintf(file, "\n");
        }
    }

    fprintf(file, "\n");

    fprintf(file, "};\n\n");

    return BOO_OK;
}

static boo_int_t
lookup_write_next(FILE *file, boo_lookup_table_t *lookup, const char *prefix)
{
    boo_int_t i, *pi;
    boo_uint_t n = 0;

    fprintf(file, "static const boo_int_t %s_next[] = {\n", prefix);

    pi = (boo_int_t*)lookup->next;

    for(i = 0 ; i != lookup->end ; i++) {
        fprintf(file, "%6d,", pi[1]);

        if(n % lookup->row_stride == lookup->row_stride - 1) {
            fprintf(file, "\n");
        }
        n++; pi += 2;
    }

    fprintf(file, "\n");

    fprintf(file, "};\n\n");

    return BOO_OK;
}

static boo_int_t
lookup_write_check(FILE *file, boo_lookup_table_t *lookup, const char *prefix)
{
    boo_int_t i, *pi;
    boo_uint_t n = 0;

    fprintf(file, "static const char %s_check[] = {\n", prefix);

    pi = (boo_int_t*)lookup->next;

    for(i = 0 ; i != lookup->end ; i++) {
        fprintf(file, "%6d,", pi[0]);

        if(n % lookup->row_stride == lookup->row_stride - 1) {
            fprintf(file, "\n");
        }
        n++; pi += 2;
    }

    fprintf(file, "\n");

    fprintf(file, "};\n\n");

    return BOO_OK;
}

boo_int_t lookup_write(FILE *file, boo_lookup_table_t *lookup, const char *prefix)
{
    boo_int_t rc = lookup_write_base(file, lookup, prefix);

    if(rc != BOO_OK) {
        return rc;
    }

    rc = lookup_write_next(file, lookup, prefix);

    if(rc != BOO_OK) {
        return rc;
    }

    return lookup_write_check(file, lookup, prefix);
}

boo_int_t lookup_index(boo_lookup_table_t *lookup) {
    boo_lookup_state_t *p, *q;
    boo_lookup_transition_t *t;
    boo_int_t base, offset;
    boo_int_t rc;
    boo_int_t *pi;

    p = lookup->states;
    q = p + lookup->num_states;

    while(p != q) {
        if(p->transitions == NULL) {
            p++;
            continue;
        }

        base = (lookup->gaps != NULL ? lookup->gaps->from : lookup->end)
            - p->transitions->input;
        
        for(offset = base ; offset != (boo_int_t)lookup->end ; offset += 1) {
            rc = lookup_can_store_transitions(lookup, offset, p);

            if(rc == BOO_ERROR) {
                return rc;
            }

            if(rc == BOO_OK) {
                break;
            }
        }

        p->base = offset;

        rc = lookup_book_space(lookup, offset, p);

        if(rc == BOO_ERROR) {
            return rc;
        }

        p++;
    }

    if(lookup->end == 0) {
        return BOO_OK;
    }

    lookup->next = pcalloc(lookup->pool, lookup->end * boo_trans_len);

    if(lookup->next == NULL) {
        return BOO_ERROR;
    }

    p = lookup->states;

    while(p != q) {
        if(p->transitions == NULL) {
            p++;
            continue;
        }

        for(t = p->transitions ; t != NULL ; t = t->next) {
            pi = (boo_int_t*)&lookup->next[p->base * boo_trans_len + t->input * boo_trans_len];

            *pi = p - lookup->states; // State number

            pi++;

            *pi = t->target;
        }

        p++;
    }

    return BOO_OK;
}
