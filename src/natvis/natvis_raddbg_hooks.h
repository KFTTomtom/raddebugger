// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_RADDBG_HOOKS_H
#define NATVIS_RADDBG_HOOKS_H

////////////////////////////////
//~ NatVis <-> RAD Debugger Glue

// Keep RAD-specific NatVis integration out of raddbg_core.c. These hooks are
// included from the raddbg layer because they depend on RAD config/state types.

internal void rd_natvis_init(void);
internal void rd_natvis_update_settings(void);
internal void rd_natvis_register_auto_hooks(Arena *arena, D_EntityArray modules, E_AutoHookMap *auto_hook_map);

#endif // NATVIS_RADDBG_HOOKS_H
