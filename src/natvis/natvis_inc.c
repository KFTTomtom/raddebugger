// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis Includes

#undef LAYER_COLOR
#define LAYER_COLOR 0x66bb6aff

#include "natvis_parse.c"
#include "natvis_types.c"
#include "natvis_eval.c"
#include "natvis_expand.c"
#include "natvis_cache.c"

#if !defined(NATVIS_STANDALONE)
#include "natvis_integration.c"
#endif
