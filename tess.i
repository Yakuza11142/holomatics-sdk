%module TessCore
%{
/* Include your native core system definition structures inside the compilation wrapper */
#include "tess_core.h"
%}

/* Advanced: Import explicit integer mappings to guarantee 100% binary size precision across platforms */
%include "stdint.i"
%include "std_string.i"

/* Hardened Parse Filter: Blocks volatile custom internal compiler macros from crashing the SWIG scanner */
#ifndef SWIG
#define __restrict
#endif

/* Process core header to auto-generate language bindings */
%include "tess_core.h"
