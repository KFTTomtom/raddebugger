// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ Editor Preferences Glue

internal void
rd_preferences_handle_font_size_cmd(RD_CmdKind kind)
{
  CFG_Node *cfg = &cfg_nil_node;
  S32 delta = 0;
  switch(kind)
  {
    case RD_CmdKind_IncWindowFontSize:
    {
      cfg = cfg_node_from_id(rd_regs()->window);
      rd_regs()->view = 0;
      rd_regs()->tab = 0;
      delta = +1;
    }break;
    case RD_CmdKind_DecWindowFontSize:
    {
      cfg = cfg_node_from_id(rd_regs()->window);
      rd_regs()->view = 0;
      rd_regs()->tab = 0;
      delta = -1;
    }break;
    case RD_CmdKind_IncViewFontSize:
    {
      cfg = cfg_node_from_id(rd_regs()->view);
      delta = +1;
    }break;
    case RD_CmdKind_DecViewFontSize:
    {
      cfg = cfg_node_from_id(rd_regs()->view);
      delta = -1;
    }break;
  }

  if(cfg != &cfg_nil_node && delta != 0)
  {
    fnt_reset();
    F32 current_font_size = rd_font_size();
    F32 new_font_size = Clamp(6.f, current_font_size + (F32)delta, 72.f);
    CFG_Node *font_size_cfg = cfg_node_child_from_string_or_alloc(rd_state->cfg, cfg, str8_lit("font_size"));
    cfg_node_new_replacef(rd_state->cfg, font_size_cfg, "%I64u", (U64)new_font_size);
  }
}

internal void
rd_preferences_push_theme_editor(Arena *arena, CFG_Node *parent)
{
  rd_cmd(RD_CmdKind_PushQuery, .expr = push_str8f(arena, "query:config.$%I64x.theme_colors", parent->id));
}

internal void
rd_preferences_add_theme_color(Arena *arena, CFG_Node *parent)
{
  Access *access = access_open();
  CFG_Node *theme = cfg_node_child_from_string_or_alloc(rd_state->cfg, parent, str8_lit("theme"));
  MD_Node *theme_tree = rd_theme_tree_from_name(arena, access, theme->first->string);
  if(theme_tree == &md_nil_node)
  {
    cfg_node_new_replace(rd_state->cfg, theme, rd_theme_preset_display_string_table[RD_ThemePreset_DefaultDark]);
  }
  CFG_Node *color = cfg_node_new(rd_state->cfg, parent, str8_lit("theme_color"));
  cfg_node_new(rd_state->cfg, color, str8_lit("tags"));
  CFG_Node *value = cfg_node_new(rd_state->cfg, color, str8_lit("value"));
  cfg_node_new(rd_state->cfg, value, str8_lit("0xffffffff"));
  access_close(access);
}

internal void
rd_preferences_fork_theme(Arena *arena, CFG_Node *parent)
{
  Access *access = access_open();
  CFG_NodePtrList colors = cfg_node_child_list_from_string(arena, parent, str8_lit("theme_color"));
  for(CFG_NodePtrNode *n = colors.first; n != 0; n = n->next)
  {
    cfg_node_release(rd_state->cfg, n->v);
  }
  CFG_Node *theme_cfg = cfg_node_child_from_string(parent, str8_lit("theme"));
  String8 theme_name = theme_cfg->first->string;
  MD_Node *theme_tree = rd_theme_tree_from_name(arena, access, theme_name);
  if(theme_tree == &md_nil_node)
  {
    theme_tree = rd_state->theme_preset_trees[RD_ThemePreset_DefaultDark];
  }
  for(MD_Node *n = theme_tree; !md_node_is_nil(n); n = md_node_rec_depth_first_pre(n, theme_tree).next)
  {
    if(str8_match(n->string, str8_lit("theme_color"), 0))
    {
      CFG_Node *color = cfg_node_new(rd_state->cfg, parent, str8_lit("theme_color"));
      CFG_Node *tags = cfg_node_new(rd_state->cfg, color, str8_lit("tags"));
      CFG_Node *value = cfg_node_new(rd_state->cfg, color, str8_lit("value"));
      cfg_node_new(rd_state->cfg, tags, md_child_from_string(n, str8_lit("tags"), 0)->first->string);
      cfg_node_new(rd_state->cfg, value, md_child_from_string(n, str8_lit("value"), 0)->first->string);
    }
  }
  cfg_node_release(rd_state->cfg, theme_cfg);
  access_close(access);
}

internal void
rd_preferences_save_theme(Arena *arena, RD_CmdKind kind, CFG_Node *parent, String8 name)
{
  if(name.size != 0)
  {
    String8 themes_folder = push_str8f(arena, "%S/raddbg/themes", get_process_info()->user_program_config_data_path);
    if(make_directory(themes_folder))
    {
      String8 dst_path = push_str8f(arena, "%S/%S", themes_folder, name);
      CFG_NodePtrList colors = cfg_node_child_list_from_string(arena, parent, str8_lit("theme_color"));
      String8List strings = {0};
      for(CFG_NodePtrNode *n = colors.first; n != 0; n = n->next)
      {
        str8_list_push(arena, &strings, cfg_string_from_tree(arena, rd_state->cfg_schema_table, str8_chop_last_slash(dst_path), n->v));
      }
      String8 data = str8_list_join(arena, &strings, 0);
      if(write_data_to_file_path(dst_path, data))
      {
        if(kind == RD_CmdKind_SaveAndSetTheme)
        {
          for(CFG_NodePtrNode *n = colors.first; n != 0; n = n->next)
          {
            cfg_node_release(rd_state->cfg, n->v);
          }
          CFG_Node *theme = cfg_node_child_from_string_or_alloc(rd_state->cfg, parent, str8_lit("theme"));
          cfg_node_new_replace(rd_state->cfg, theme, name);
        }
      }
      else
      {
        log_user_errorf("Could not successfully write to '%S'.", dst_path);
      }
    }
  }
}

internal void
rd_preferences_handle_theme_cmd(Arena *arena, RD_CmdKind kind)
{
  switch(kind)
  {
    case RD_CmdKind_EditUserTheme:
    {
      CFG_Node *parent = cfg_node_child_from_string(cfg_node_root(), str8_lit("user"));
      rd_preferences_push_theme_editor(arena, parent);
    }break;
    case RD_CmdKind_EditProjectTheme:
    {
      CFG_Node *parent = cfg_node_child_from_string(cfg_node_root(), str8_lit("project"));
      rd_preferences_push_theme_editor(arena, parent);
    }break;
    case RD_CmdKind_AddThemeColor:
    {
      rd_preferences_add_theme_color(arena, cfg_node_from_id(rd_regs()->cfg));
    }break;
    case RD_CmdKind_ForkTheme:
    {
      rd_preferences_fork_theme(arena, cfg_node_from_id(rd_regs()->cfg));
    }break;
    case RD_CmdKind_SaveTheme:
    case RD_CmdKind_SaveAndSetTheme:
    {
      rd_preferences_save_theme(arena, kind, cfg_node_from_id(rd_regs()->cfg), rd_regs()->string);
    }break;
  }
}
