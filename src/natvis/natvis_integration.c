// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis Global State

internal NV_State *
nv_state_alloc(void)
{
  Arena *arena = arena_alloc();
  NV_State *state = push_array(arena, NV_State, 1);
  state->arena = arena;
  state->cache = nv_cache_alloc();
  state->initialized = 1;
  state->reload_interval_us = 2000000; // 2 seconds
  return state;
}

internal void
nv_state_release(NV_State *state)
{
  if(state == 0) { return; }
  nv_cache_release(state->cache);
  arena_release(state->arena);
}

////////////////////////////////
//~ Configuration

internal void
nv_state_add_search_path(NV_State *state, String8 path)
{
  str8_list_push(state->arena, &state->search_paths, str8_copy(state->arena, path));
}

internal void
nv_state_load_file(NV_State *state, String8 path)
{
  nv_cache_load_file(state->cache, path);
}

internal B32
nv_state_dir_already_scanned(NV_State *state, String8 dir_path)
{
  for(NV_ScannedDir *d = state->first_scanned_dir; d != 0; d = d->next)
  {
    if(str8_match(d->path, dir_path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
    {
      return 1;
    }
  }
  return 0;
}

internal void
nv_state_load_directory(NV_State *state, String8 dir_path)
{
  if(dir_path.size == 0) { return; }
  if(nv_state_dir_already_scanned(state, dir_path)) { return; }
  
  // record as scanned
  NV_ScannedDir *sd = push_array(state->arena, NV_ScannedDir, 1);
  sd->path = str8_copy(state->arena, dir_path);
  SLLQueuePush(state->first_scanned_dir, state->last_scanned_dir, sd);
  
  Temp scratch = temp_begin(state->arena);
  
  OS_FileIter *it = os_file_iter_begin(scratch.arena, dir_path, OS_FileIterFlag_SkipFolders);
  for(OS_FileInfo info = {0}; os_file_iter_next(scratch.arena, it, &info); )
  {
    if(info.name.size > 7)
    {
      String8 ext = str8(info.name.str + info.name.size - 7, 7);
      if(str8_match(ext, str8_lit(".natvis"), StringMatchFlag_CaseInsensitive))
      {
        String8 full_path = push_str8f(state->arena, "%S/%S", dir_path, info.name);
        nv_cache_load_file(state->cache, full_path);
      }
    }
  }
  os_file_iter_end(it);
  
  temp_end(scratch);
}

internal void
nv_state_load_from_string(NV_State *state, String8 xml_data, String8 source_name)
{
  nv_cache_load_from_string(state->cache, xml_data, source_name, NV_SourceKind_Embedded);
}

////////////////////////////////
//~ Intrinsic Mapping Table
//
// Maps UE NatVis <Intrinsic> function calls to RAD eval member expressions.
// These are Visual Studio-specific virtual functions that NatVis types call
// but which have no debugger-side implementation in RAD. Instead, we know
// the underlying C++ member structure and can provide equivalent expressions.

typedef struct NV_IntrinsicMapping NV_IntrinsicMapping;
struct NV_IntrinsicMapping
{
  String8 type_prefix;
  String8 intrinsic_name;
  String8 rad_expression;
};

global read_only NV_IntrinsicMapping nv_intrinsic_table[] =
{
  { str8_lit_comp("TArray"),   str8_lit_comp("_Num"),     str8_lit_comp("ArrayNum") },
  { str8_lit_comp("TArray"),   str8_lit_comp("_Max"),     str8_lit_comp("ArrayMax") },
  { str8_lit_comp("TArray"),   str8_lit_comp("_GetData"), str8_lit_comp("AllocatorInstance.Data") },
  { str8_lit_comp("FString"),  str8_lit_comp("_Num"),     str8_lit_comp("Data.ArrayNum") },
  { str8_lit_comp("FString"),  str8_lit_comp("_Max"),     str8_lit_comp("Data.ArrayMax") },
  { str8_lit_comp("FString"),  str8_lit_comp("_GetData"), str8_lit_comp("Data.AllocatorInstance.Data") },
  { str8_lit_comp("TSet"),     str8_lit_comp("_Num"),     str8_lit_comp("Elements.NumFreeIndices == 0 ? Elements.Data.NumBits : Elements.Data.NumBits - Elements.NumFreeIndices") },
  { str8_lit_comp("TMap"),     str8_lit_comp("_Num"),     str8_lit_comp("Pairs.Elements.NumFreeIndices == 0 ? Pairs.Elements.Data.NumBits : Pairs.Elements.Data.NumBits - Pairs.Elements.NumFreeIndices") },
};

internal String8
nv_intrinsic_lookup(String8 type_name, String8 intrinsic_name)
{
  for(U64 i = 0; i < ArrayCount(nv_intrinsic_table); i += 1)
  {
    if(str8_match(str8_prefix(type_name, nv_intrinsic_table[i].type_prefix.size),
                  nv_intrinsic_table[i].type_prefix, 0) &&
       str8_match(intrinsic_name, nv_intrinsic_table[i].intrinsic_name, 0))
    {
      return nv_intrinsic_table[i].rad_expression;
    }
  }
  return str8_zero();
}

////////////////////////////////
//~ Registration into RAD Eval System

internal void
nv_register_auto_hooks(NV_State *state, Arena *arena, E_AutoHookMap *auto_hook_map)
{
  if(state == 0 || state->cache == 0 || auto_hook_map == 0) { return; }
  
  Temp scratch = temp_begin(arena);
  
  for(NV_CacheEntry *entry = state->cache->first; entry != 0; entry = entry->next)
  {
    NV_File *nv_file = entry->file;
    if(nv_file == 0) { continue; }
    
    for(NV_TypeDef *td = nv_file->first_type; td != 0; td = td->next)
    {
      // skip types without any displayable content
      if(td->first_display_string == 0 && td->expand == 0) { continue; }
      
      // convert the NatVis type pattern to RAD eval pattern syntax:
      // NatVis uses * for wildcards, RAD eval uses ?
      String8 pattern = td->name;
      
      // translate * → ? for RAD eval pattern matching
      {
        String8List parts = {0};
        U64 i = 0;
        while(i < pattern.size)
        {
          if(pattern.str[i] == '*')
          {
            str8_list_push(scratch.arena, &parts, str8_lit("?"));
            i += 1;
          }
          else
          {
            U64 start = i;
            while(i < pattern.size && pattern.str[i] != '*') { i += 1; }
            str8_list_push(scratch.arena, &parts, str8(pattern.str + start, i - start));
          }
        }
        pattern = str8_list_join(scratch.arena, &parts, 0);
      }
      
      // try to match template args for the expression generation
      // (use empty args for non-template types)
      String8 dummy_template_args[16] = {0};
      U64 template_count = 0;
      
      // count wildcards in the pattern to set up placeholder $T references
      for(U64 i = 0; i < td->name.size; i += 1)
      {
        if(td->name.str[i] == '*' && template_count < ArrayCount(dummy_template_args))
        {
          dummy_template_args[template_count] = push_str8f(scratch.arena, "$T%llu", template_count + 1);
          template_count += 1;
        }
      }
      
      // generate the tag expression and inline intrinsic calls
      String8 tag_expr = nv_type_view_expr_from_typedef(scratch.arena, td, dummy_template_args, template_count);
      tag_expr = nv_inline_intrinsic_calls(scratch.arena, tag_expr, td->first_intrinsic, nv_file->first_intrinsic);
      
      // generate the summary expression from DisplayString
      String8 summary_expr = nv_summary_expr_from_typedef(scratch.arena, td, dummy_template_args, template_count);
      summary_expr = nv_inline_intrinsic_calls(scratch.arena, summary_expr, td->first_intrinsic, nv_file->first_intrinsic);
      
      if(tag_expr.size > 0)
      {
        e_auto_hook_map_insert_new(arena, auto_hook_map,
          .type_pattern = str8_copy(arena, pattern),
          .tag_expr_string = str8_copy(arena, tag_expr),
          .summary_expr_string = summary_expr.size > 0 ? str8_copy(arena, summary_expr) : str8_zero());
      }
      
      // also register AlternativeType names
      for(String8Node *alt = td->alternative_names.first; alt != 0; alt = alt->next)
      {
        String8 alt_pattern = alt->string;
        {
          String8List parts = {0};
          U64 i = 0;
          while(i < alt_pattern.size)
          {
            if(alt_pattern.str[i] == '*')
            {
              str8_list_push(scratch.arena, &parts, str8_lit("?"));
              i += 1;
            }
            else
            {
              U64 start = i;
              while(i < alt_pattern.size && alt_pattern.str[i] != '*') { i += 1; }
              str8_list_push(scratch.arena, &parts, str8(alt_pattern.str + start, i - start));
            }
          }
          alt_pattern = str8_list_join(scratch.arena, &parts, 0);
        }
        
        e_auto_hook_map_insert_new(arena, auto_hook_map,
          .type_pattern = str8_copy(arena, alt_pattern),
          .tag_expr_string = str8_copy(arena, tag_expr),
          .summary_expr_string = summary_expr.size > 0 ? str8_copy(arena, summary_expr) : str8_zero());
      }
    }
  }
  
  temp_end(scratch);
}

////////////////////////////////
//~ Hot Reload

internal U64
nv_check_reload(NV_State *state)
{
  if(state == 0 || state->cache == 0) { return 0; }
  
  U64 now = os_now_microseconds();
  if(state->last_reload_check_us != 0 &&
     (now - state->last_reload_check_us) < state->reload_interval_us)
  {
    return 0;
  }
  state->last_reload_check_us = now;
  return nv_cache_hot_reload(state->cache);
}

////////////////////////////////
//~ Type Lookup

internal NV_TypeDef *
nv_find_type(NV_State *state, String8 type_name, NV_TypeMatch *out_match)
{
  if(state == 0 || state->cache == 0) { return 0; }
  return nv_cache_find_type(state->cache, type_name, out_match);
}

internal NV_TypeDef *
nv_find_type_ex(NV_State *state, String8 type_name, NV_TypeMatch *out_match, NV_File **out_file)
{
  if(state == 0 || state->cache == 0) { return 0; }
  return nv_cache_find_type_ex(state->cache, type_name, out_match, out_file);
}
