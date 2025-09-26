cmake_minimum_required(VERSION 3.5)
project(moh)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_BUILD_TYPE Debug)

#
# Set strictest warning level
#
if(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-long-long -pedantic")
endif()

set(INCLUDES "")
check_include_files(unistd.h HAVE_UNISTD_H)

if(HAVE_UNISTD_H)
  set(INCLUDES ${INCLUDES} unistd.h)
endif()

check_symbol_exists(sysconf "${INCLUDES}" HAVE_SYSCONF)
check_symbol_exists(getpagesize "${INCLUDES}" HAVE_GETPAGESIZE)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(lalr1_SOURCES
    string.c
    list.c
    vector.c
    pool.c
    trie.c
    symtab.c
    grammar.c
    lookahead.c
    lookup.c
    output.c
    )

set(lalr1_HEADERS
    boo.h
    string.h
    list.h
    vector.h
    pool.h
    trie.h
    symtab.h
    grammar.h
    lookahead.h
    lookup.h
    output.h
    )

add_library(lalr1 ${lalr1_SOURCES})
add_executable(bootstrap main.c bootstrap.c)
target_link_libraries(bootstrap lalr1)

add_custom_command(
  OUTPUT moh_grammar.h
  COMMAND bootstrap ${CMAKE_CURRENT_SOURCE_DIR}/moh_grammar.moh
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/moh_grammar.moh
  VERBATIM
)
add_custom_target(generate_moh_grammar_h DEPENDS moh_grammar.h)
set_target_properties(generate_moh_grammar_h PROPERTIES ADDITIONAL_CLEAN_FILES moh_grammar.debug)

add_executable(moh main.c bootstrap.c)
add_dependencies(moh generate_moh_grammar_h)
target_link_libraries(moh lalr1)
