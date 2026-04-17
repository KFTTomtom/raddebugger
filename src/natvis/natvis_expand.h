// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_EXPAND_H
#define NATVIS_EXPAND_H

////////////////////////////////
//~ NatVis Expansion — output items from expansion evaluation

typedef struct NV_OutputItem NV_OutputItem;
struct NV_OutputItem
{
  NV_OutputItem *next;
  String8 name;
  String8 expression;
};

typedef struct NV_OutputItemList NV_OutputItemList;
struct NV_OutputItemList
{
  NV_OutputItem *first;
  NV_OutputItem *last;
  U64 count;
};

////////////////////////////////
//~ NatVis Expansion — evaluate an Expand block into output items

internal NV_OutputItemList nv_expand_items_from_expand(
  Arena *arena,
  NV_Expand *expand,
  String8 *template_args,
  U64 template_arg_count
);

////////////////////////////////
//~ NatVis CustomListItems Interpreter

#define NV_CL_MAX_ITERATIONS 100000
#define NV_CL_DEFAULT_MAX_ITEMS 5000

typedef struct NV_CLRuntimeVar NV_CLRuntimeVar;
struct NV_CLRuntimeVar
{
  NV_CLRuntimeVar *next;
  String8 name;
  String8 value_expr;
};

typedef struct NV_CLInterpreter NV_CLInterpreter;
struct NV_CLInterpreter
{
  Arena *arena;
  NV_CLRuntimeVar *first_var;
  NV_CLRuntimeVar *last_var;
  NV_OutputItemList output;
  U64 max_items;
  U64 total_iterations;
  B32 break_requested;
  String8 *template_args;
  U64 template_arg_count;
};

internal NV_OutputItemList nv_cl_interpret(
  Arena *arena,
  NV_CustomListItemsData *data,
  String8 *template_args,
  U64 template_arg_count
);

#endif // NATVIS_EXPAND_H
