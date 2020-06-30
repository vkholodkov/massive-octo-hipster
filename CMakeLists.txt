project(moh C)
cmake_minimum_required(VERSION 3.5)

include_directories(src mustach)

set(SOURCES
        src/bootstrap.c
        src/grammar.c
        src/list.c
        src/lookahead.c
        src/lookup.c
        src/output.c
        src/pool.c
        src/bstring.c
        src/symtab.c
        src/trie.c
        src/vector.c
        src/mustach-moh.c
        mustach/mustach.c
        )
set(HEADERS
        src/boo.h
        src/grammar.h
        src/list.h
        src/lookahead.h
        src/lookup.h
        src/output.h
        src/pool.h
        src/bstring.h
        src/symtab.h
        src/trie.h
        src/vector.h
        src/mustach-moh.h
        mustach/mustach.h
        )
add_executable(moh src/main.c ${SOURCES} ${HEADERS})
