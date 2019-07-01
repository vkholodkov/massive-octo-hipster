
Massive octo hipster
====================

Massive octo hipster (moh) is a parser generation and language recognition tool. Currently
it supports LALR(1) parser generation using YACC grammar syntax.

This is a research project in a prototype phase. Many things are subject to change, including
names, syntax and functionality.

How to build
--------------

Checkout the source from the repository and run autoconf in the top directory.

After that run:

    ./configure
    make depend
    make

This should produce a binary src/moh. To install run:

    make install

This will install the binary into your bin path.


How to run
----------

Run moh on a grammar file to get a header file and a debug file with the same name.

For example:

    ./src/moh src/examples/expr.moh

produces files expr.h and expr.debug. test.c shows how to use generated files.


Frequently asked questions
--------------------------

Q: Why another tool?
A: This is a research project that came out of my study of LR parsers and grammars.
   I used BISON and I have poor experience in producing parsers that suit all needs
   of my code. This tool was concived to be flexible enough to fit into complex
   situations such as asynchronous web servers.

Q: What does the name stand for?
A: I used github project suggestion tool to generate a name and it came up with Massive
   octo hipster. In my experience content fills the name and not the other way around.
   The name sounds cool, so let's fill it with something as cool as the name.

Credits
--------------------------

Copyright (C) 2015-2019 Coldrift Technologies B.V.

[Visit company's website](http://coldrift.com)

