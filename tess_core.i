%module TessCore
%{
#include "app/src/main/cpp/tesseract_engine.h"
%}

/* Import explicit size primitives safely */
%include "stdint.i"
%include "std_string.i"

/* 
   Hardened Rule block: Explicitly force SWIG to define TESS_API as a blank token 
   before reading any files, thoroughly wiping out syntax error input panics.
*/
#define TESS_API

/* Scan the target header file directly */
%include "app/src/main/cpp/tesseract_engine.h"
