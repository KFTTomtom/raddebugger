// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis <-> RAD Debugger Glue

internal B32 rd_setting_b32_from_name(String8 name);

global NV_State *rd_natvis_state = 0;
global B32 rd_natvis_enabled = 0;

internal void
rd_natvis_init(void)
{
  rd_natvis_state = nv_state_alloc();
}

internal void
rd_natvis_update_settings(void)
{
  rd_natvis_enabled = rd_setting_b32_from_name(str8_lit("use_natvis"));
}

internal void
rd_natvis_load_module_directories(Arena *arena, NV_State *state, D_EntityArray modules)
{
  for(U64 module_idx = 0; module_idx < modules.count; module_idx += 1)
  {
    D_Entity *module = modules.v[module_idx];
    if(module != 0 && module->string.size > 0)
    {
      String8 dir = str8_chop_last_slash(module->string);
      if(dir.size > 0)
      {
        nv_state_load_directory(state, dir);

        // scan Engine\Extras\VisualStudioDebugging\ when a UE module is detected
        if(str8_find_needle(module->string, 0, str8_lit("Engine\\Binaries"), StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive) < module->string.size ||
           str8_find_needle(module->string, 0, str8_lit("UnrealEditor"), StringMatchFlag_CaseInsensitive) < module->string.size)
        {
          String8 engine_root = dir;
          for(U64 attempt = 0; attempt < 4; attempt += 1)
          {
            String8 candidate = push_str8f(arena, "%S/Extras/VisualStudioDebugging", engine_root);
            if(folder_path_exists(candidate))
            {
              nv_state_load_directory(state, candidate);
              break;
            }
            engine_root = str8_chop_last_slash(engine_root);
            if(engine_root.size == 0) { break; }
          }
        }
      }
    }
  }
}

internal void
rd_natvis_load_configured_paths(Arena *arena, NV_State *state)
{
  CFG_NodePtrList natvis_paths = cfg_node_top_level_list_from_string(arena, str8_lit("natvis_path"));
  for(CFG_NodePtrNode *n = natvis_paths.first; n != 0; n = n->next)
  {
    String8 path = n->v->first->string;
    if(path.size > 0)
    {
      nv_state_load_directory(state, path);
    }
  }
}

internal void
rd_natvis_register_auto_hooks(Arena *arena, D_EntityArray modules, E_AutoHookMap *auto_hook_map)
{
  if(rd_natvis_enabled && rd_natvis_state != 0 && auto_hook_map != 0)
  {
    rd_natvis_load_module_directories(arena, rd_natvis_state, modules);
    rd_natvis_load_configured_paths(arena, rd_natvis_state);
    nv_check_reload(rd_natvis_state);
    nv_register_auto_hooks(rd_natvis_state, arena, auto_hook_map);
  }
}
