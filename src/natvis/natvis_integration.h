// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_INTEGRATION_H
#define NATVIS_INTEGRATION_H

////////////////////////////////
//~ NatVis Integration with RAD Debugger
//
// This module bridges the NatVis system with the RAD Debugger's
// eval visualization infrastructure.
//
// It provides functions to:
// 1. Load .natvis files from disk paths, PDB embedded sources, or directories
// 2. Register type visualizers from NatVis into the E_AutoHookMap
// 3. Manage a global NatVis cache with hot-reload support

////////////////////////////////
//~ NatVis Global State

typedef struct NV_State NV_State;
struct NV_State
{
  Arena *arena;
  NV_Cache *cache;
  String8List search_paths;
  B32 initialized;
  U64 last_reload_check_us;
  U64 reload_interval_us;
};

////////////////////////////////
//~ NatVis Integration API

// lifecycle
internal NV_State *nv_state_alloc(void);
internal void      nv_state_release(NV_State *state);

// configuration
internal void nv_state_add_search_path(NV_State *state, String8 path);
internal void nv_state_load_file(NV_State *state, String8 path);
internal void nv_state_load_directory(NV_State *state, String8 dir_path);
internal void nv_state_load_from_string(NV_State *state, String8 xml_data, String8 source_name);

// registration into RAD eval system
// Call this per-frame (or on change) to populate auto_hook_map entries
// from all loaded NatVis type definitions.
internal void nv_register_auto_hooks(NV_State *state, Arena *arena, E_AutoHookMap *auto_hook_map);

// hot-reload check (call periodically)
internal U64 nv_check_reload(NV_State *state);

// type lookup (debug/inspection)
internal NV_TypeDef *nv_find_type(NV_State *state, String8 type_name, NV_TypeMatch *out_match);

#endif // NATVIS_INTEGRATION_H
