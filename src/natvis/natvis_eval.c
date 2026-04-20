// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis Intrinsic Call Inlining — helpers

internal B32
nv_is_ident_char(U8 c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_');
}

// Parse a comma-separated argument list between balanced parens.
// Caller must ensure expr.str[0] == '('. Returns argument strings and
// advances *out_end past the closing ')'.
internal U64
nv_parse_call_args(String8 expr, U64 open_paren, String8 *out_args, U64 max_args, U64 *out_end)
{
  U64 count = 0;
  U64 i = open_paren + 1; // skip '('
  U64 depth = 1;
  U64 arg_start = i;
  
  while(i < expr.size && depth > 0)
  {
    U8 c = expr.str[i];
    if(c == '(' || c == '<' || c == '[') { depth += 1; }
    else if(c == ')' || c == '>' || c == ']')
    {
      depth -= 1;
      if(depth == 0)
      {
        // closing paren: capture last argument
        String8 arg = str8_skip_chop_whitespace(str8(expr.str + arg_start, i - arg_start));
        if(arg.size > 0 && count < max_args)
        {
          out_args[count] = arg;
          count += 1;
        }
        *out_end = i + 1;
        return count;
      }
    }
    else if(c == ',' && depth == 1)
    {
      String8 arg = str8_skip_chop_whitespace(str8(expr.str + arg_start, i - arg_start));
      if(count < max_args)
      {
        out_args[count] = arg;
        count += 1;
      }
      arg_start = i + 1;
    }
    i += 1;
  }
  
  // unbalanced parens: treat as no match
  *out_end = i;
  return count;
}

// Substitute parameter names with argument values in an intrinsic body.
internal String8
nv_substitute_intrinsic_params(Arena *arena, String8 body, NV_Intrinsic *intr, String8 *args, U64 arg_count)
{
  String8List parts = {0};
  B32 any_sub = 0;
  U64 i = 0;
  
  while(i < body.size)
  {
    B32 found = 0;
    U64 param_idx = 0;
    for(NV_IntrinsicParam *p = intr->first_param; p != 0; p = p->next, param_idx += 1)
    {
      if(param_idx >= arg_count) { break; }
      if(p->name.size == 0) { continue; }
      if(i + p->name.size > body.size) { continue; }
      
      String8 window = str8(body.str + i, p->name.size);
      if(!str8_match(window, p->name, 0)) { continue; }
      
      B32 left_ok = (i == 0) || !nv_is_ident_char(body.str[i - 1]);
      B32 right_ok = (i + p->name.size >= body.size) || !nv_is_ident_char(body.str[i + p->name.size]);
      
      if(left_ok && right_ok)
      {
        str8_list_push(arena, &parts, str8_lit("("));
        str8_list_push(arena, &parts, args[param_idx]);
        str8_list_push(arena, &parts, str8_lit(")"));
        i += p->name.size;
        found = 1;
        any_sub = 1;
        break;
      }
    }
    
    if(!found)
    {
      str8_list_push(arena, &parts, str8(body.str + i, 1));
      i += 1;
    }
  }
  
  if(!any_sub) { return body; }
  return str8_list_join(arena, &parts, 0);
}

// Single-pass inline: scan for identifier( patterns, match against intrinsics
internal String8
nv_inline_intrinsic_calls_once(Arena *arena, String8 expr, NV_Intrinsic *type_intrinsics, NV_Intrinsic *global_intrinsics, B32 *did_inline)
{
  *did_inline = 0;
  String8List parts = {0};
  U64 i = 0;
  
  while(i < expr.size)
  {
    // strip "this->" prefix before potential intrinsic name
    U64 call_start = i;
    B32 had_this = 0;
    if(i + 6 <= expr.size && MemoryCompare(expr.str + i, "this->", 6) == 0)
    {
      had_this = 1;
    }
    
    // look for an identifier
    U64 id_start = had_this ? i + 6 : i;
    if(id_start < expr.size && nv_is_ident_char(expr.str[id_start]) &&
       !(expr.str[id_start] >= '0' && expr.str[id_start] <= '9'))
    {
      U64 id_end = id_start;
      while(id_end < expr.size && nv_is_ident_char(expr.str[id_end])) { id_end += 1; }
      String8 name = str8(expr.str + id_start, id_end - id_start);
      
      // must not be preceded by an ident char (unless it's "this->")
      B32 left_ok = had_this || (call_start == 0) || !nv_is_ident_char(expr.str[call_start - 1]);
      
      if(left_ok)
      {
        // check for '(' right after identifier (skip whitespace)
        U64 after_name = id_end;
        while(after_name < expr.size && (expr.str[after_name] == ' ' || expr.str[after_name] == '\t')) { after_name += 1; }
        
        if(after_name < expr.size && expr.str[after_name] == '(')
        {
          // try to find matching intrinsic
          NV_Intrinsic *found = nv_intrinsic_from_name(type_intrinsics, name);
          if(found == 0) { found = nv_intrinsic_from_name(global_intrinsics, name); }
          
          if(found != 0)
          {
            String8 call_args[16] = {0};
            U64 call_end = 0;
            U64 arg_count = nv_parse_call_args(expr, after_name, call_args, ArrayCount(call_args), &call_end);
            
            // substitute params in the intrinsic body
            String8 inlined = nv_substitute_intrinsic_params(arena, found->expression, found, call_args, arg_count);
            
            // wrap in parens to preserve evaluation order
            str8_list_push(arena, &parts, str8_lit("("));
            str8_list_push(arena, &parts, inlined);
            str8_list_push(arena, &parts, str8_lit(")"));
            
            i = call_end;
            *did_inline = 1;
            continue;
          }
        }
        
        // also handle no-arg intrinsic called without parens: "this->name" at end
        // or followed by non-identifier (e.g. "->", ".", ",")
        if(had_this)
        {
          NV_Intrinsic *found = nv_intrinsic_from_name(type_intrinsics, name);
          if(found == 0) { found = nv_intrinsic_from_name(global_intrinsics, name); }
          
          if(found != 0 && found->param_count == 0 &&
             (after_name >= expr.size || expr.str[after_name] != '('))
          {
            str8_list_push(arena, &parts, str8_lit("("));
            str8_list_push(arena, &parts, found->expression);
            str8_list_push(arena, &parts, str8_lit(")"));
            i = id_end;
            *did_inline = 1;
            continue;
          }
        }
      }
    }
    
    // no intrinsic match: emit current character
    str8_list_push(arena, &parts, str8(expr.str + i, 1));
    i += 1;
  }
  
  if(!(*did_inline)) { return expr; }
  return str8_list_join(arena, &parts, 0);
}

internal String8
nv_inline_intrinsic_calls(Arena *arena, String8 expr, NV_Intrinsic *type_intrinsics, NV_Intrinsic *global_intrinsics)
{
  if(expr.size == 0) { return expr; }
  if(type_intrinsics == 0 && global_intrinsics == 0) { return expr; }
  
  String8 result = expr;
  for(U64 depth = 0; depth < NV_INTRINSIC_MAX_RECURSION; depth += 1)
  {
    B32 did_inline = 0;
    result = nv_inline_intrinsic_calls_once(arena, result, type_intrinsics, global_intrinsics, &did_inline);
    if(!did_inline) { break; }
  }
  return result;
}

////////////////////////////////
//~ NatVis Expression Translation

internal String8
nv_translate_expr(Arena *arena, String8 natvis_expr, String8 *template_args, U64 template_arg_count)
{
  if(natvis_expr.size == 0) { return natvis_expr; }
  
  Temp scratch = temp_begin(arena);
  String8List parts = {0};
  U64 i = 0;
  
  while(i < natvis_expr.size)
  {
    // $T1..$T9 template parameter substitution
    if(natvis_expr.str[i] == '$' && i + 2 < natvis_expr.size && natvis_expr.str[i+1] == 'T')
    {
      U8 digit = natvis_expr.str[i+2];
      if(digit >= '1' && digit <= '9')
      {
        U64 idx = digit - '1';
        if(idx < template_arg_count && template_args[idx].size > 0)
        {
          str8_list_push(arena, &parts, template_args[idx]);
          i += 3;
          continue;
        }
      }
    }
    
    // $e → $ (parent/self reference) — used in some NatVis variants
    if(natvis_expr.str[i] == '$' && i + 1 < natvis_expr.size && natvis_expr.str[i+1] == 'e')
    {
      // check it's not $en or another identifier
      B32 is_end = (i + 2 >= natvis_expr.size);
      if(!is_end)
      {
        U8 next = natvis_expr.str[i+2];
        is_end = !((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9') || next == '_');
      }
      if(is_end)
      {
        str8_list_push(arena, &parts, str8_lit("$"));
        i += 2;
        continue;
      }
    }
    
    // C-style cast: (Type *)expr → cast(Type *)expr
    // detect: '(' followed by an identifier and then '*' or '&' then ')'
    if(natvis_expr.str[i] == '(')
    {
      // save position and try to detect a C-style cast
      U64 paren_start = i;
      U64 j = i + 1;
      
      // skip whitespace
      while(j < natvis_expr.size && (natvis_expr.str[j] == ' ' || natvis_expr.str[j] == '\t')) { j += 1; }
      
      // read potential type name (identifiers, ::, *, &, spaces, <, >, commas for templates)
      U64 type_start = j;
      U64 angle_depth = 0;
      B32 has_star_or_amp = 0;
      B32 looks_like_type = 0;
      
      while(j < natvis_expr.size)
      {
        U8 c = natvis_expr.str[j];
        if(c == '<') { angle_depth += 1; j += 1; continue; }
        if(c == '>' && angle_depth > 0) { angle_depth -= 1; j += 1; continue; }
        if(angle_depth > 0) { j += 1; continue; }
        
        if(c == '*' || c == '&') { has_star_or_amp = 1; j += 1; continue; }
        if(c == ')' && has_star_or_amp)
        {
          looks_like_type = 1;
          break;
        }
        if(c == ')') { break; }
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == ':' || c == ' ' || c == '\t' || c == ',')
        {
          j += 1;
          continue;
        }
        break;
      }
      
      if(looks_like_type && j < natvis_expr.size && natvis_expr.str[j] == ')')
      {
        String8 type_text = str8(natvis_expr.str + type_start, j - type_start);
        str8_list_push(arena, &parts, str8_lit("cast("));
        str8_list_push(arena, &parts, type_text);
        str8_list_push(arena, &parts, str8_lit(")"));
        i = j + 1;
        continue;
      }
    }
    
    // regular character passthrough
    str8_list_push(arena, &parts, str8(natvis_expr.str + i, 1));
    i += 1;
  }
  
  String8 result = str8_list_join(arena, &parts, 0);
  temp_end(scratch);
  return result;
}

////////////////////////////////
//~ Format Specifier Application

internal String8
nv_apply_format_spec(Arena *arena, String8 expr, String8 format_spec)
{
  if(format_spec.size == 0) { return expr; }
  
  if(str8_match(format_spec, str8_lit("x"), StringMatchFlag_CaseInsensitive) ||
     str8_match(format_spec, str8_lit("X"), StringMatchFlag_CaseInsensitive) ||
     str8_match(format_spec, str8_lit("h"), StringMatchFlag_CaseInsensitive) ||
     str8_match(format_spec, str8_lit("H"), StringMatchFlag_CaseInsensitive))
  {
    return push_str8f(arena, "hex(%S)", expr);
  }
  if(str8_match(format_spec, str8_lit("b"), StringMatchFlag_CaseInsensitive) ||
     str8_match(format_spec, str8_lit("bb"), StringMatchFlag_CaseInsensitive))
  {
    return push_str8f(arena, "bin(%S)", expr);
  }
  if(str8_match(format_spec, str8_lit("o"), StringMatchFlag_CaseInsensitive))
  {
    return push_str8f(arena, "oct(%S)", expr);
  }
  
  // ,s / ,su / ,s8 / ,s32 etc. → treat as string (no transform needed,
  // RAD debugger handles char* display natively)
  // ,d → decimal (default, no change)
  // ,na / ,nd / ,en / ,! → metadata hints, pass through
  
  return expr;
}

////////////////////////////////
//~ NatVis DisplayString → tag expression

internal String8
nv_display_string_to_tag_expr(Arena *arena, NV_DisplayString *ds, String8 *template_args, U64 template_arg_count)
{
  if(ds == 0) { return str8_zero(); }
  
  // if the DisplayString is a single expression part with no literal text,
  // return that expression directly as the tag expr (common case: {expr})
  U64 part_count = 0;
  NV_DisplayPart *single_expr = 0;
  for(NV_DisplayPart *p = ds->first_part; p != 0; p = p->next)
  {
    part_count += 1;
    if(p->kind == NV_DisplayPartKind_Expression) { single_expr = p; }
  }
  
  if(part_count == 1 && single_expr != 0)
  {
    String8 translated = nv_translate_expr(arena, single_expr->text, template_args, template_arg_count);
    return nv_apply_format_spec(arena, translated, single_expr->format_spec);
  }
  
  // for multi-part DisplayStrings, find the first expression part
  // and use that as the primary tag expression
  // (full DisplayString rendering requires UI integration — future work)
  for(NV_DisplayPart *p = ds->first_part; p != 0; p = p->next)
  {
    if(p->kind == NV_DisplayPartKind_Expression)
    {
      String8 translated = nv_translate_expr(arena, p->text, template_args, template_arg_count);
      return nv_apply_format_spec(arena, translated, p->format_spec);
    }
  }
  
  return str8_zero();
}

////////////////////////////////
//~ NatVis TypeDef → type_view expression

internal String8
nv_summary_expr_from_typedef(Arena *arena, NV_TypeDef *td, String8 *template_args, U64 template_arg_count)
{
  if(td == 0) { return str8_zero(); }
  
  for(NV_DisplayString *ds = td->first_display_string; ds != 0; ds = ds->next)
  {
    if(ds->condition.size != 0) { continue; }
    for(NV_DisplayPart *p = ds->first_part; p != 0; p = p->next)
    {
      if(p->kind == NV_DisplayPartKind_Expression)
      {
        String8 translated = nv_translate_expr(arena, p->text, template_args, template_arg_count);
        return nv_apply_format_spec(arena, translated, p->format_spec);
      }
    }
  }
  
  if(td->first_display_string != 0)
  {
    for(NV_DisplayPart *p = td->first_display_string->first_part; p != 0; p = p->next)
    {
      if(p->kind == NV_DisplayPartKind_Expression)
      {
        String8 translated = nv_translate_expr(arena, p->text, template_args, template_arg_count);
        return nv_apply_format_spec(arena, translated, p->format_spec);
      }
    }
  }
  
  return str8_zero();
}

internal String8
nv_type_view_expr_from_typedef(Arena *arena, NV_TypeDef *td, String8 *template_args, U64 template_arg_count)
{
  if(td == 0) { return str8_zero(); }
  
  // priority 1: if there is an Expand with a single ArrayItems, generate slice/array expression
  if(td->expand != 0)
  {
    for(NV_ExpandItem *ei = td->expand->first_item; ei != 0; ei = ei->next)
    {
      if(ei->kind == NV_ExpandItemKind_ArrayItems)
      {
        String8 size_e = nv_translate_expr(arena, ei->array_items.size_expr, template_args, template_arg_count);
        String8 ptr_e = nv_translate_expr(arena, ei->array_items.value_pointer_expr, template_args, template_arg_count);
        if(size_e.size > 0 && ptr_e.size > 0)
        {
          return push_str8f(arena, "array(%S, %S)", ptr_e, size_e);
        }
      }
    }
  }
  
  // priority 2: DisplayString (first unconditional one)
  for(NV_DisplayString *ds = td->first_display_string; ds != 0; ds = ds->next)
  {
    if(ds->condition.size == 0)
    {
      String8 tag = nv_display_string_to_tag_expr(arena, ds, template_args, template_arg_count);
      if(tag.size > 0) { return tag; }
    }
  }
  
  // priority 3: first DisplayString even if conditional
  if(td->first_display_string != 0)
  {
    String8 tag = nv_display_string_to_tag_expr(arena, td->first_display_string, template_args, template_arg_count);
    if(tag.size > 0) { return tag; }
  }
  
  return str8_zero();
}
