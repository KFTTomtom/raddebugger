// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_CACHE_H
#define NATVIS_CACHE_H

////////////////////////////////
//~ NatVis Cache — source tracking

typedef enum NV_SourceKind
{
  NV_SourceKind_File,
  NV_SourceKind_PDB,
  NV_SourceKind_Embedded,
  NV_SourceKind_COUNT,
} NV_SourceKind;

typedef struct NV_CacheEntry NV_CacheEntry;
struct NV_CacheEntry
{
  NV_CacheEntry *next;
  NV_CacheEntry *prev;
  String8 source_path;
  U64 source_timestamp;
  NV_SourceKind source_kind;
  NV_File *file;
  Arena *arena;
};

typedef struct NV_Cache NV_Cache;
struct NV_Cache
{
  Arena *arena;
  NV_CacheEntry *first;
  NV_CacheEntry *last;
  U64 count;
};

////////////////////////////////
//~ NatVis Cache — Functions

internal NV_Cache *nv_cache_alloc(void);
internal void      nv_cache_release(NV_Cache *cache);

internal NV_File * nv_cache_load_file(NV_Cache *cache, String8 path);
internal NV_File * nv_cache_load_from_string(NV_Cache *cache, String8 xml_data, String8 source_name, NV_SourceKind source_kind);
internal void      nv_cache_remove_by_path(NV_Cache *cache, String8 path);

internal NV_TypeDef *nv_cache_find_type(NV_Cache *cache, String8 type_name, NV_TypeMatch *out_match);
internal NV_TypeDef *nv_cache_find_type_ex(NV_Cache *cache, String8 type_name, NV_TypeMatch *out_match, NV_File **out_file);

// hot-reload: check file timestamps, re-parse changed files
internal U64 nv_cache_hot_reload(NV_Cache *cache);

#endif // NATVIS_CACHE_H
