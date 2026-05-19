// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RADDBG_PREFERENCES_HOOKS_H
#define RADDBG_PREFERENCES_HOOKS_H

////////////////////////////////
//~ Editor Preferences Glue

// Keep KFT editor preference commands out of the main command switch, so
// upstream raddbg_core.c remains easier to merge.

internal void rd_preferences_handle_font_size_cmd(RD_CmdKind kind);
internal void rd_preferences_handle_theme_cmd(Arena *arena, RD_CmdKind kind);

#endif // RADDBG_PREFERENCES_HOOKS_H
