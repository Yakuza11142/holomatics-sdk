/* Dynamic SWIG Interface Wrapper */
#ifndef MODULE_NAME
#define MODULE_NAME TessCore
#endif

%module MODULE_NAME

%{
/* Dynamic inclusion of target header */
#ifdef HEADER_FILE
#include HEADER_FILE
#else
#include "tess_core.h"
#endif
%}

/* Advanced: Import explicit integer and string mappings for target platforms */
%include "stdint.i"
%include "std_string.i"
%include "cpointer.i"

/* Hardened Parse Filter: Blocks volatile custom internal compiler macros */
#ifndef SWIG
#define __restrict
#define __attribute__(x)
#endif

/* Dynamic Header Parsing Phase */
#ifdef HEADER_FILE
%include HEADER_FILE
#else
%include "tess_core.h"
#endif
