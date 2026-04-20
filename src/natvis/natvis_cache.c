// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis Cache

internal NV_Cache *
nv_cache_alloc(void)
{
  Arena *arena = arena_alloc();
  NV_Cache *cache = push_array(arena, NV_Cache, 1);
  cache->arena = arena;
  return cache;
}

internal void
nv_cache_release(NV_Cache *cache)
{
  if(cache == 0) { return; }
  // release per-entry arenas
  for(NV_CacheEntry *e = cache->first; e != 0; e = e->next)
  {
    if(e->arena != cache->arena)
    {
      arena_release(e->arena);
    }
  }
  arena_release(cache->arena);
}

internal NV_File *
nv_cache_load_file(NV_Cache *cache, String8 path)
{
  // check if already loaded
  for(NV_CacheEntry *e = cache->first; e != 0; e = e->next)
  {
    if(str8_match(e->source_path, path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
    {
      return e->file;
    }
  }
  
  // read file
  OS_Handle file_handle = os_file_open(OS_AccessFlag_Read, path);
  FileProperties props = os_properties_from_file(file_handle);
  if(props.size == 0)
  {
    os_file_close(file_handle);
    return 0;
  }
  
  Arena *entry_arena = arena_alloc();
  String8 file_data = {0};
  file_data.size = props.size;
  file_data.str = push_array(entry_arena, U8, file_data.size);
  os_file_read(file_handle, r1u64(0, file_data.size), file_data.str);
  os_file_close(file_handle);
  
  // parse
  NV_XMLParseResult xr = nv_xml_parse_from_string(entry_arena, file_data);
  NV_File *nv_file = nv_file_from_xml(entry_arena, xr.root, path);
  
  // create cache entry
  NV_CacheEntry *entry = push_array(cache->arena, NV_CacheEntry, 1);
  entry->source_path = str8_copy(cache->arena, path);
  entry->source_timestamp = props.modified;
  entry->source_kind = NV_SourceKind_File;
  entry->file = nv_file;
  entry->arena = entry_arena;
  DLLPushBack(cache->first, cache->last, entry);
  cache->count += 1;
  
  return nv_file;
}

internal NV_File *
nv_cache_load_from_string(NV_Cache *cache, String8 xml_data, String8 source_name, NV_SourceKind source_kind)
{
  Arena *entry_arena = arena_alloc();
  String8 data_copy = str8_copy(entry_arena, xml_data);
  
  NV_XMLParseResult xr = nv_xml_parse_from_string(entry_arena, data_copy);
  NV_File *nv_file = nv_file_from_xml(entry_arena, xr.root, source_name);
  
  NV_CacheEntry *entry = push_array(cache->arena, NV_CacheEntry, 1);
  entry->source_path = str8_copy(cache->arena, source_name);
  entry->source_timestamp = 0;
  entry->source_kind = source_kind;
  entry->file = nv_file;
  entry->arena = entry_arena;
  DLLPushBack(cache->first, cache->last, entry);
  cache->count += 1;
  
  return nv_file;
}

internal void
nv_cache_remove_by_path(NV_Cache *cache, String8 path)
{
  for(NV_CacheEntry *e = cache->first; e != 0; e = e->next)
  {
    if(str8_match(e->source_path, path, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
    {
      DLLRemove(cache->first, cache->last, e);
      cache->count -= 1;
      if(e->arena != cache->arena)
      {
        arena_release(e->arena);
      }
      break;
    }
  }
}

internal NV_TypeDef *
nv_cache_find_type_ex(NV_Cache *cache, String8 type_name, NV_TypeMatch *out_match, NV_File **out_file)
{
  if(cache == 0) { return 0; }
  
  NV_TypeDef *best = 0;
  NV_TypeMatch best_match = {0};
  NV_Priority best_priority = NV_Priority_Low;
  NV_File *best_file = 0;
  
  for(NV_CacheEntry *e = cache->first; e != 0; e = e->next)
  {
    NV_File *f = e->file;
    if(f == 0) { continue; }
    
    for(NV_TypeDef *td = f->first_type; td != 0; td = td->next)
    {
      NV_TypeMatch m = nv_type_match(td->name, type_name);
      if(m.matched && td->priority >= best_priority)
      {
        best = td;
        best_match = m;
        best_priority = td->priority;
        best_file = f;
        continue;
      }
      
      for(String8Node *alt = td->alternative_names.first; alt != 0; alt = alt->next)
      {
        NV_TypeMatch am = nv_type_match(alt->string, type_name);
        if(am.matched && td->priority >= best_priority)
        {
          best = td;
          best_match = am;
          best_priority = td->priority;
          best_file = f;
        }
      }
    }
  }
  
  if(out_match != 0 && best != 0) { *out_match = best_match; }
  if(out_file != 0) { *out_file = best_file; }
  return best;
}

internal NV_TypeDef *
nv_cache_find_type(NV_Cache *cache, String8 type_name, NV_TypeMatch *out_match)
{
  return nv_cache_find_type_ex(cache, type_name, out_match, 0);
}

internal U64
nv_cache_hot_reload(NV_Cache *cache)
{
  U64 reloaded = 0;
  if(cache == 0) { return 0; }
  
  for(NV_CacheEntry *e = cache->first; e != 0; e = e->next)
  {
    if(e->source_kind != NV_SourceKind_File) { continue; }
    
    OS_Handle fh = os_file_open(OS_AccessFlag_Read, e->source_path);
    FileProperties props = os_properties_from_file(fh);
    os_file_close(fh);
    
    if(props.modified != e->source_timestamp && props.size > 0)
    {
      // re-parse
      if(e->arena != cache->arena)
      {
        arena_release(e->arena);
      }
      
      Arena *new_arena = arena_alloc();
      OS_Handle fh2 = os_file_open(OS_AccessFlag_Read, e->source_path);
      String8 data = {0};
      data.size = props.size;
      data.str = push_array(new_arena, U8, data.size);
      os_file_read(fh2, r1u64(0, data.size), data.str);
      os_file_close(fh2);
      
      NV_XMLParseResult xr = nv_xml_parse_from_string(new_arena, data);
      e->file = nv_file_from_xml(new_arena, xr.root, e->source_path);
      e->arena = new_arena;
      e->source_timestamp = props.modified;
      reloaded += 1;
    }
  }
  
  return reloaded;
}
