// KFT-specific eval visualization hooks. Keep this file in the
// eval_visualization layer: it may use eval/eval_visualization APIs, but must
// not depend on raddbg UI internals.

internal B32
kft_ev_type_key_matches_unreal_object_name(E_TypeKey type_key)
{
  B32 result = 0;
  if(!e_type_key_match(type_key, e_type_key_zero()))
  {
    E_Type *type = e_type_from_key(type_key);
    if(type != 0 && type != &e_type_nil && type->kind != E_TypeKind_Null)
    {
      Temp scratch = scratch_begin(0, 0);
      String8 type_name = e_type_string_from_key(scratch.arena, type_key);
      result = str8_match(type_name, str8_lit("UObject"), 0);
      if(!result && type_name.size > str8_lit("::UObject").size)
      {
        result = str8_match(str8_skip(type_name, type_name.size - str8_lit("::UObject").size), str8_lit("::UObject"), 0);
      }
      scratch_end(scratch);
    }
  }
  return result;
}

internal B32
kft_ev_type_key_is_or_inherits_from_unreal_object__walk(E_TypeKey type_key, U32 max_depth, E_TypeKey *visited_keys, U64 visited_count, U64 visited_cap)
{
  B32 result = 0;
  if(max_depth != 0)
  {
    E_TypeKey check_key = e_type_key_unwrap(type_key, E_TypeUnwrapFlag_AllDecorative);
    if(!e_type_key_match(check_key, e_type_key_zero()))
    {
      for(U64 visited_idx = 0; visited_idx < visited_count; visited_idx += 1)
      {
        if(e_type_key_match(visited_keys[visited_idx], check_key))
        {
          return 0;
        }
      }
      
      E_Type *type = e_type_from_key(check_key);
      if(type != 0 && type != &e_type_nil && type->kind != E_TypeKind_Null)
      {
        if(kft_ev_type_key_matches_unreal_object_name(check_key))
        {
          result = 1;
        }
        else if(visited_count < visited_cap &&
                (type->kind == E_TypeKind_Struct ||
                 type->kind == E_TypeKind_Class ||
                 type->kind == E_TypeKind_Union) &&
                type->members != 0)
        {
          visited_keys[visited_count] = check_key;
          visited_count += 1;
          
          U64 member_count = Min(type->count, 4096);
          for(U64 member_idx = 0; member_idx < member_count && !result; member_idx += 1)
          {
            E_Member *member = &type->members[member_idx];
            if((member->kind == E_MemberKind_Base ||
                member->kind == E_MemberKind_VirtualBase) &&
               !e_type_key_match(member->type_key, e_type_key_zero()))
            {
              result = kft_ev_type_key_is_or_inherits_from_unreal_object__walk(member->type_key, max_depth-1, visited_keys, visited_count, visited_cap);
            }
          }
        }
      }
    }
  }
  return result;
}

internal B32
kft_ev_type_key_is_or_inherits_from_unreal_object(E_TypeKey type_key, U32 max_depth)
{
  E_TypeKey visited_keys[64] = {0};
  U32 capped_depth = Min(max_depth, ArrayCount(visited_keys));
  B32 result = kft_ev_type_key_is_or_inherits_from_unreal_object__walk(type_key, capped_depth, visited_keys, 0, ArrayCount(visited_keys));
  return result;
}

internal B32
kft_ev_type_key_is_unreal_object_pointer_or_ref(E_TypeKey type_key)
{
  E_TypeKey ptr_type_key = e_type_key_unwrap(type_key, E_TypeUnwrapFlag_AllDecorative);
  B32 result = 0;
  if(!e_type_key_match(ptr_type_key, e_type_key_zero()))
  {
    E_TypeKind ptr_type_kind = e_type_kind_from_key(ptr_type_key);
    if(e_type_kind_is_pointer_or_ref(ptr_type_kind))
    {
      E_TypeKey ptee_type_key = e_type_key_unwrap(e_type_key_direct(ptr_type_key), E_TypeUnwrapFlag_AllDecorative);
      if(!e_type_key_match(ptee_type_key, e_type_key_zero()))
      {
        result = kft_ev_type_key_is_or_inherits_from_unreal_object(ptee_type_key, 32);
      }
    }
  }
  return result;
}

internal B32
kft_ev_pointer_should_emit_address_first(EV_StringParams *params, E_TypeKind type_kind)
{
  B32 result = ((params->flags & EV_StringFlag_ReadOnlyDisplayRules) &&
                (type_kind == E_TypeKind_Ptr ||
                 type_kind == E_TypeKind_LRef ||
                 type_kind == E_TypeKind_RRef));
  return result;
}

internal B32
kft_ev_pointer_should_descend_inline(E_TypeKind type_kind)
{
  B32 result = (type_kind == E_TypeKind_Array);
  return result;
}

internal U64
kft_ev_top_level_pos_from_string(String8 string, U8 needle)
{
  U64 result = string.size;
  U64 paren_depth = 0;
  U64 bracket_depth = 0;
  U64 brace_depth = 0;
  U64 angle_depth = 0;
  for(U64 idx = 0; idx < string.size; idx += 1)
  {
    U8 byte = string.str[idx];
    if(0){}
    else if(byte == '(') { paren_depth += 1; }
    else if(byte == '[') { bracket_depth += 1; }
    else if(byte == '{') { brace_depth += 1; }
    else if(byte == '<') { angle_depth += 1; }
    else if(byte == ')' && paren_depth > 0) { paren_depth -= 1; }
    else if(byte == ']' && bracket_depth > 0) { bracket_depth -= 1; }
    else if(byte == '}' && brace_depth > 0) { brace_depth -= 1; }
    else if(byte == '>' && angle_depth > 0) { angle_depth -= 1; }
    else if(byte == needle && paren_depth == 0 && bracket_depth == 0 &&
            brace_depth == 0 && angle_depth == 0)
    {
      result = idx;
      break;
    }
  }
  return result;
}

internal String8
kft_ev_summary_item_from_string(String8 string, U64 *offset_io)
{
  U64 start = *offset_io;
  for(; start < string.size && char_is_space(string.str[start]); start += 1);
  String8 remaining = str8_skip(string, start);
  U64 comma_pos = kft_ev_top_level_pos_from_string(remaining, ',');
  U64 opl = start + comma_pos;
  *offset_io = Min(string.size, opl + 1);
  String8 result = str8_skip_chop_whitespace(str8_substr(string, r1u64(start, opl)));
  return result;
}

internal String8
kft_ev_string_from_auto_hook_summary(Arena *arena, EV_StringParams *params, E_Eval eval, String8 summary_string)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  U64 item_count = 0;
  B32 has_named_item = 0;
  for(U64 offset = 0; offset < summary_string.size;)
  {
    String8 item = kft_ev_summary_item_from_string(summary_string, &offset);
    if(item.size != 0)
    {
      item_count += 1;
      if(kft_ev_top_level_pos_from_string(item, '=') < item.size)
      {
        has_named_item = 1;
      }
    }
  }
  
  String8List strings = {0};
  if(item_count > 1 || has_named_item)
  {
    str8_list_push(scratch.arena, &strings, str8_lit("{"));
  }
  
  U64 item_idx = 0;
  for(U64 offset = 0; offset < summary_string.size;)
  {
    String8 item = kft_ev_summary_item_from_string(summary_string, &offset);
    if(item.size == 0)
    {
      continue;
    }
    
    U64 eq_pos = kft_ev_top_level_pos_from_string(item, '=');
    String8 name = {0};
    String8 expr = item;
    if(eq_pos < item.size)
    {
      name = str8_skip_chop_whitespace(str8_prefix(item, eq_pos));
      expr = str8_skip_chop_whitespace(str8_skip(item, eq_pos+1));
    }
    
    if(item_idx != 0)
    {
      str8_list_push(scratch.arena, &strings, str8_lit(", "));
    }
    if(has_named_item && name.size != 0)
    {
      str8_list_pushf(scratch.arena, &strings, "%S=", name);
    }
    
    EV_StringParams value_params = *params;
    value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
    E_Eval summary_eval = e_eval_wrap(eval, expr);
    String8 value_string = ev_value_string_from_eval(scratch.arena, &value_params, summary_eval, 256);
    str8_list_push(scratch.arena, &strings, value_string);
    item_idx += 1;
  }
  
  if(item_count > 1 || has_named_item)
  {
    str8_list_push(scratch.arena, &strings, str8_lit("}"));
  }
  
  String8 result = str8_list_join(arena, &strings, 0);
  scratch_end(scratch);
  return result;
}

internal String8
kft_ev_string_from_default_struct_summary(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  E_TypeKey original_type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  E_TypeKind original_type_kind = e_type_kind_from_key(original_type_key);
  if(e_type_kft_show_properties_like_vs() &&
     !e_type_kind_is_pointer_or_ref(original_type_kind))
  {
    Temp scratch = scratch_begin(&arena, 1);
    E_TypeKey expand_type_key = e_default_expansion_type_from_key(eval.irtree.type_key);
    E_TypeKind expand_type_kind = e_type_kind_from_key(expand_type_key);
    if(expand_type_kind == E_TypeKind_Struct ||
       expand_type_kind == E_TypeKind_Class ||
       expand_type_kind == E_TypeKind_Union)
    {
      E_MemberArray data_members = e_type_data_members_from_key_filter__cached(expand_type_key, params->filter);
      String8List strings = {0};
      U64 visible_count = 0;
      U64 emitted_count = 0;
      str8_list_push(scratch.arena, &strings, str8_lit("{"));
      for(U64 member_idx = 0; member_idx < data_members.count && visible_count < 8; member_idx += 1)
      {
        E_Member *member = &data_members.v[member_idx];
        if(member->name.size == 0 || e_type_kft_member_name_is_padding_noise(member->name))
        {
          continue;
        }
        
        if(emitted_count != 0)
        {
          str8_list_push(scratch.arena, &strings, str8_lit(", "));
        }
        str8_list_pushf(scratch.arena, &strings, "%S=", member->name);
        
        E_TypeKey unwrapped_type_key = e_type_key_unwrap(member->type_key, E_TypeUnwrapFlag_AllDecorative);
        E_TypeKind member_type_kind = e_type_kind_from_key(unwrapped_type_key);
        if(e_type_kind_is_basic_or_enum(member_type_kind) ||
           e_type_kind_is_pointer_or_ref(member_type_kind))
        {
          EV_StringParams value_params = *params;
          value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
          E_Eval member_eval = e_eval_wrapf(eval, "$.%S", member->name);
          String8 value_string = ev_value_string_from_eval(scratch.arena, &value_params, member_eval, 256);
          str8_list_push(scratch.arena, &strings, value_string);
        }
        else
        {
          String8 type_string = e_type_string_from_key(scratch.arena, unwrapped_type_key);
          str8_list_push(scratch.arena, &strings, type_string);
        }
        
        visible_count += 1;
        emitted_count += 1;
      }
      if(data_members.count > visible_count)
      {
        if(emitted_count != 0)
        {
          str8_list_push(scratch.arena, &strings, str8_lit(", "));
        }
        str8_list_push(scratch.arena, &strings, str8_lit("..."));
      }
      str8_list_push(scratch.arena, &strings, str8_lit("}"));
      if(emitted_count != 0)
      {
        result = str8_list_join(arena, &strings, 0);
      }
    }
    scratch_end(scratch);
  }
  return result;
}

internal String8
kft_ev_string_from_unreal_object_pointer_properties(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  Temp scratch = scratch_begin(&arena, 1);
  EV_StringParams value_params = *params;
  value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
  value_params.flags |= EV_StringFlag_DisableAddresses;
  
  String8List strings = {0};
  U64 emitted_count = 0;
  str8_list_push(scratch.arena, &strings, str8_lit("{"));
  
  String8 name_string = {0};
  E_Eval name_eval = e_eval_wrapf(eval, "(*$).NamePrivate");
  if(name_eval.msgs.max_kind == E_MsgKind_Null)
  {
    name_string = ev_value_string_from_eval(scratch.arena, &value_params, name_eval, 256);
  }
  if(name_string.size != 0)
  {
    str8_list_pushf(scratch.arena, &strings, "NamePrivate=%S", name_string);
    emitted_count += 1;
  }
  
  String8 outer_string = {0};
  E_Eval outer_eval = e_eval_wrapf(eval, "(*$).OuterPrivate");
  if(outer_eval.msgs.max_kind == E_MsgKind_Null)
  {
    E_Eval outer_value_eval = e_value_eval_from_eval(outer_eval);
    if(outer_value_eval.value.u64 != 0)
    {
      E_Eval outer_name_eval = e_eval_wrapf(eval, "(*$).OuterPrivate->NamePrivate");
      if(outer_name_eval.msgs.max_kind == E_MsgKind_Null)
      {
        outer_string = ev_value_string_from_eval(scratch.arena, &value_params, outer_name_eval, 256);
      }
    }
    else
    {
      outer_string = str8_lit("0");
    }
  }
  if(outer_string.size != 0)
  {
    if(emitted_count != 0)
    {
      str8_list_push(scratch.arena, &strings, str8_lit(", "));
    }
    str8_list_pushf(scratch.arena, &strings, "OuterPrivate=%S", outer_string);
    emitted_count += 1;
  }
  
  str8_list_push(scratch.arena, &strings, str8_lit("}"));
  String8 result = {0};
  if(emitted_count != 0)
  {
    result = str8_list_join(arena, &strings, 0);
  }
  scratch_end(scratch);
  return result;
}

internal String8
kft_ev_string_from_pointer_properties(Arena *arena, EV_StringParams *params, E_Eval eval, String8 ptr_value_string, B32 is_unreal_object_ptr_or_ref)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  String8 properties_string = {0};
  E_Eval deref_eval = e_eval_wrapf(eval, "*$");
  if(deref_eval.msgs.max_kind == E_MsgKind_Null)
  {
    if(is_unreal_object_ptr_or_ref)
    {
      properties_string = kft_ev_string_from_unreal_object_pointer_properties(scratch.arena, params, eval);
    }
    if(properties_string.size == 0)
    {
      properties_string = kft_ev_string_from_default_struct_summary(scratch.arena, params, deref_eval);
    }
  }
  
  String8 result = ptr_value_string;
  if(properties_string.size != 0)
  {
    result = push_str8f(arena, "%S %S", ptr_value_string, properties_string);
  }
  else
  {
    result = push_str8_copy(arena, ptr_value_string);
  }
  
  scratch_end(scratch);
  return result;
}
