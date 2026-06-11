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
  if(state->hook_map_arena != 0)
  {
    arena_release(state->hook_map_arena);
    state->hook_map_arena = 0;
  }
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

  FileIter *it = file_iter_begin(scratch.arena, dir_path, FileIterFlag_SkipFolders);
  for(FileInfo info = {0}; file_iter_next(scratch.arena, it, &info); )
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
  file_iter_end(it);

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
//~ Expression Safety Filter
//
// RAD's eval parser does not support C library function calls (strstr, etc.),
// certain casts (uintptr_t), or deeply nested inlined intrinsics.
// Expressions containing these constructs can crash the parser.

internal B32
nv_expr_is_safe_for_rad(String8 expr, String8 pattern, B32 log_rejection)
{
  if(expr.size == 0 || expr.str == 0) { return 0; }

  String8 unsafe_needles[] = {
    str8_lit_comp("strstr("),
    str8_lit_comp("strlen("),
    str8_lit_comp("strcmp("),
    str8_lit_comp("memcmp("),
    str8_lit_comp("sizeof("),
    str8_lit_comp("offsetof("),
    str8_lit_comp("uintptr_t"),
    str8_lit_comp("intptr_t"),
    str8_lit_comp("nullptr"),
  };
  for(U64 i = 0; i < ArrayCount(unsafe_needles); i += 1)
  {
    if(str8_find_needle(expr, 0, unsafe_needles[i], 0) < expr.size)
    {
      if(log_rejection)
      {
        log_infof("natvis: SKIP \"%.*s\" — expr contains unsupported \"%.*s\"\n",
          str8_varg(pattern), str8_varg(unsafe_needles[i]));
      }
      return 0;
    }
  }

  return 1;
}

////////////////////////////////
//~ Static Constexpr Substitution
//
// RAD cannot resolve static constexpr values from PDB symbols.
// We apply known substitutions before registering hooks.

typedef struct NV_ConstexprSub NV_ConstexprSub;
struct NV_ConstexprSub
{
  String8 token;
  String8 value;
};

global read_only NV_ConstexprSub nv_constexpr_table[] =
{
  { str8_lit_comp("FNameDebugVisualizer::EntryStride"), str8_lit_comp("8")          },
  { str8_lit_comp("FNameDebugVisualizer::OffsetBits"),  str8_lit_comp("16")         },
  { str8_lit_comp("FNameDebugVisualizer::OffsetMask"),  str8_lit_comp("0xFFFF")     },
  { str8_lit_comp("FNameDebugVisualizer::UnusedMask"),  str8_lit_comp("0x1FFF0000") },
  { str8_lit_comp("FNameDebugVisualizer::MaxLength"),   str8_lit_comp("1024")       },
};

internal String8
nv_apply_constexpr_subs(Arena *arena, String8 expr)
{
  if(expr.size == 0) { return expr; }
  B32 changed = 0;
  String8 result = expr;
  for(U64 i = 0; i < ArrayCount(nv_constexpr_table); i += 1)
  {
    U64 pos = str8_find_needle(result, 0, nv_constexpr_table[i].token, 0);
    if(pos < result.size)
    {
      String8List parts = {0};
      U64 cursor = 0;
      while(cursor < result.size)
      {
        U64 found = str8_find_needle(result, cursor, nv_constexpr_table[i].token, 0);
        if(found < result.size)
        {
          str8_list_push(arena, &parts, str8(result.str + cursor, found - cursor));
          str8_list_push(arena, &parts, nv_constexpr_table[i].value);
          cursor = found + nv_constexpr_table[i].token.size;
        }
        else
        {
          str8_list_push(arena, &parts, str8(result.str + cursor, result.size - cursor));
          break;
        }
      }
      result = str8_list_join(arena, &parts, 0);
      changed = 1;
    }
  }
  return result;
}

////////////////////////////////
//~ Registration into RAD Eval System

internal void
nv_build_auto_hook_map_from_cached_hooks(NV_State *state)
{
  if(state->hook_map_arena == 0)
  {
    state->hook_map_arena = arena_alloc();
  }
  arena_clear(state->hook_map_arena);
  MemoryZeroStruct(&state->natvis_auto_hook_map);
  U64 slot_count = state->cached_hook_count*2;
  slot_count = Clamp(512, slot_count, 65536);
  state->natvis_auto_hook_map = e_auto_hook_map_make(state->hook_map_arena, slot_count);
  for(NV_CachedHook *h = state->first_cached_hook; h != 0; h = h->next)
  {
    if(h->pattern.size == 0 || h->pattern.str == 0) { continue; }
    if(h->tag_expr.size == 0 || h->tag_expr.str == 0) { continue; }
    e_auto_hook_map_insert_new(state->hook_map_arena, &state->natvis_auto_hook_map,
      .type_pattern = h->pattern,
      .tag_expr_string = h->tag_expr);
  }
  state->natvis_auto_hook_map_valid = 1;
}

internal void
nv_rebuild_cached_hooks(NV_State *state)
{
  state->first_cached_hook = 0;
  state->last_cached_hook = 0;
  state->cached_hook_count = 0;
  state->hooks_logged = 0;
  state->natvis_auto_hook_map_valid = 0;

  Arena *work = arena_alloc();

  for(NV_CacheEntry *entry = state->cache->first; entry != 0; entry = entry->next)
  {
    NV_File *nv_file = entry->file;
    if(nv_file == 0) { continue; }

    for(NV_TypeDef *td = nv_file->first_type; td != 0; td = td->next)
    {
      if(td->first_display_string == 0 && td->expand == 0) { continue; }

      String8 pattern = td->name;

      String8 template_args[16] = {0};
      U64 template_count = 0;
      for(U64 i = 0; i < td->name.size; i += 1)
      {
        if(td->name.str[i] == '*' && template_count < ArrayCount(template_args))
        {
          template_args[template_count] = push_str8f(work, "T%llu", template_count + 1);
          template_count += 1;
        }
      }

      // translate * → ?{T1}, ?{T2}, etc. for RAD eval pattern matching
      {
        String8List parts = {0};
        U64 wildcard_idx = 0;
        U64 i = 0;
        while(i < pattern.size)
        {
          if(pattern.str[i] == '*')
          {
            if(wildcard_idx < template_count)
            {
              str8_list_push(work, &parts,
                push_str8f(work, "?{%S}", template_args[wildcard_idx]));
              wildcard_idx += 1;
            }
            else
            {
              str8_list_push(work, &parts, str8_lit("?"));
            }
            i += 1;
          }
          else
          {
            U64 start = i;
            while(i < pattern.size && pattern.str[i] != '*') { i += 1; }
            str8_list_push(work, &parts, str8(pattern.str + start, i - start));
          }
        }
        pattern = str8_list_join(work, &parts, 0);
      }

      String8 tag_expr = nv_type_view_expr_from_typedef(work, td, template_args, template_count);
      tag_expr = nv_inline_intrinsic_calls(work, tag_expr, td->first_intrinsic, nv_file->first_intrinsic);
      tag_expr = nv_apply_constexpr_subs(work, tag_expr);

      String8 summary_expr = nv_summary_expr_from_typedef(work, td, template_args, template_count);
      summary_expr = nv_inline_intrinsic_calls(work, summary_expr, td->first_intrinsic, nv_file->first_intrinsic);
      summary_expr = nv_apply_constexpr_subs(work, summary_expr);

      if(tag_expr.size > 0 && nv_expr_is_safe_for_rad(tag_expr, pattern, 1))
      {
        if(!nv_expr_is_safe_for_rad(summary_expr, pattern, 0))
        {
          summary_expr = str8_zero();
        }
        NV_CachedHook *hook = push_array(state->arena, NV_CachedHook, 1);
        hook->pattern = str8_copy(state->arena, pattern);
        hook->tag_expr = str8_copy(state->arena, tag_expr);
        hook->summary_expr = summary_expr.size > 0 ? str8_copy(state->arena, summary_expr) : str8_zero();
        SLLQueuePush(state->first_cached_hook, state->last_cached_hook, hook);
        state->cached_hook_count += 1;

        log_infof("[Natvis Parse] \"%.*s\" : %.*s\n",
          str8_varg(pattern), str8_varg(tag_expr));
      }

      for(String8Node *alt = td->alternative_names.first; alt != 0; alt = alt->next)
      {
        String8 alt_pattern = alt->string;
        {
          String8List parts = {0};
          U64 alt_wildcard_idx = 0;
          U64 i = 0;
          while(i < alt_pattern.size)
          {
            if(alt_pattern.str[i] == '*')
            {
              if(alt_wildcard_idx < template_count)
              {
                str8_list_push(work, &parts,
                  push_str8f(work, "?{%S}", template_args[alt_wildcard_idx]));
                alt_wildcard_idx += 1;
              }
              else
              {
                str8_list_push(work, &parts, str8_lit("?"));
              }
              i += 1;
            }
            else
            {
              U64 start = i;
              while(i < alt_pattern.size && alt_pattern.str[i] != '*') { i += 1; }
              str8_list_push(work, &parts, str8(alt_pattern.str + start, i - start));
            }
          }
          alt_pattern = str8_list_join(work, &parts, 0);
        }

        NV_CachedHook *hook = push_array(state->arena, NV_CachedHook, 1);
        hook->pattern = str8_copy(state->arena, alt_pattern);
        hook->tag_expr = str8_copy(state->arena, tag_expr);
        hook->summary_expr = summary_expr.size > 0 ? str8_copy(state->arena, summary_expr) : str8_zero();
        SLLQueuePush(state->first_cached_hook, state->last_cached_hook, hook);
        state->cached_hook_count += 1;

        log_infof("[Natvis Parse] \"%.*s\" : %.*s\n",
          str8_varg(alt_pattern), str8_varg(tag_expr));
      }
    }
  }

  state->cache_generation_at_build = state->cache->generation;
  nv_build_auto_hook_map_from_cached_hooks(state);
  arena_release(work);
}

internal void
nv_register_auto_hooks(NV_State *state, Arena *arena, E_AutoHookMap *auto_hook_map)
{
  if(state == 0 || state->cache == 0 || auto_hook_map == 0) { return; }

  // rebuild cached hooks if the cache has changed (new files loaded or hot-reloaded)
  if(state->cache_generation_at_build != state->cache->generation)
  {
    nv_rebuild_cached_hooks(state);
  }

  if(!state->natvis_auto_hook_map_valid)
  {
    nv_build_auto_hook_map_from_cached_hooks(state);
  }

  if(!state->hooks_logged)
  {
    log_infof("natvis: replaying %llu cached hooks\n", state->cached_hook_count);
    state->hooks_logged = 1;
  }

  for(NV_CachedHook *h = state->first_cached_hook; h != 0; h = h->next)
  {
    if(h->pattern.size == 0 || h->tag_expr.size == 0) { continue; }
    e_auto_hook_map_insert_new(arena, auto_hook_map,
      .type_pattern = h->pattern,
      .tag_expr_string = h->tag_expr);
  }
}

////////////////////////////////
//~ Hot Reload

internal U64
nv_check_reload(NV_State *state)
{
  if(state == 0 || state->cache == 0) { return 0; }

  U64 now = now_time_us();
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
