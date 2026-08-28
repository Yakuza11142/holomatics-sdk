%module TessCore
%{
/* 
   ============================================================================
   NATIVE COMPILATION WRAPPER ENVELOPE
   ============================================================================
   Includes the direct low-level engine runtime headers inside the generated 
   compilation unit wrapper.
*/
#include "app/src/main/cpp/tesseract_engine.h"
%}

/* 
   ============================================================================
   ADVANCED SIMD & STL TYPE ALLOCATION MAPPINGS
   ============================================================================
   Forces SWIG to pull exact multi-platform standard integers and memory sizes, 
   ensuring 100% binary size precision for cross-language Marshalling.
*/
%include "stdint.i"
%include "std_string.i"

/* 
   ============================================================================
   HARDENED COMPILER PARSE FILTERS
   ============================================================================
   Strips out volatile custom internal compiler storage keywords and qualifiers 
   that natively crash or confuse the SWIG interface scanner.
*/
#ifndef SWIG
#define __restrict
#define __attribute__(x)
#define alignas(x)
#define TESS_API
#endif

/* 
   ============================================================================
   CORE INTERFACE GENERATION MANDATE
   ============================================================================
   Scans your structural header signatures directly to auto-generate identical 
   interop bindings for C#, Python, and Java pipelines.
*/
%include "app/src/main/cpp/tesseract_engine.h"
