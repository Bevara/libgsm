/*
 *  One libc symbol that libgsm references and solver_minimal_1 does not
 *  export. A side module whose imports are not all resolved never instantiates
 *  at all - and emscripten reports it by crashing inside its own
 *  reportUndefinedSymbols, which names nothing - so it has to be defined here.
 */

#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func)
{
	fprintf(stderr, "[GSMDec] assertion failed: %s at %s:%u in %s\n",
	        expr ? expr : "?", file ? file : "?", line, func ? func : "?");
	abort();
}
