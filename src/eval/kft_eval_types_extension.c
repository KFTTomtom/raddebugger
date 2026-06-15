// KFT: VS-like property display keeps debugger-noise padding out of object expansions.
internal B32 rd_setting_b32_from_name(String8 name);

internal B32
e_type_kft_show_properties_like_vs(void)
{
  B32 result = rd_setting_b32_from_name(str8_lit("show_properties_like_vs"));
  return result;
}

internal B32
e_type_kft_member_name_is_padding_noise(String8 name)
{
  B32 result = (str8_match(name, str8_lit("$.padding"), 0) ||
                str8_match(name, str8_lit("$padding"), 0) ||
                str8_match(name, str8_lit("padding"), 0) ||
                str8_match(str8_prefix(name, 10), str8_lit("$padding_"), 0) ||
                str8_match(str8_prefix(name, 8), str8_lit("padding_"), 0));
  return result;
}

typedef enum E_KFTVSPropertyItemKind
{
  E_KFTVSPropertyItemKind_Field,
  E_KFTVSPropertyItemKind_Base,
}
E_KFTVSPropertyItemKind;

typedef struct E_KFTVSPropertyItem E_KFTVSPropertyItem;
struct E_KFTVSPropertyItem
{
  E_KFTVSPropertyItemKind kind;
  U64 id;
  U64 off;
  E_TypeKey type_key;
  String8 name;
};

typedef struct E_KFTVSPropertiesAccel E_KFTVSPropertiesAccel;
struct E_KFTVSPropertiesAccel
{
  E_KFTVSPropertyItem *items;
  U64 count;
};

internal U64
e_type_kft_vs_property_item_id(E_KFTVSPropertyItemKind kind, String8 name, U64 off, E_TypeKey type_key)
{
  U64 result = e_hash_from_string(5381, str8_struct(&kind));
  result = e_hash_from_string(result, name);
  result = e_hash_from_string(result, str8_struct(&off));
  result = e_hash_from_string(result, str8_struct(&type_key));
  result |= 1;
  return result;
}

internal String8
e_type_kft_simple_type_name_from_key(Arena *arena, E_TypeKey type_key)
{
  String8 result = e_type_string_from_key(arena, type_key);
  U64 last_colon_pos = result.size;
  for(U64 pos = str8_find_needle(result, 0, str8_lit("::"), 0);
      pos < result.size;
      pos = str8_find_needle(result, pos + 2, str8_lit("::"), 0))
  {
    last_colon_pos = pos;
  }
  if(last_colon_pos != result.size)
  {
    result = str8_skip(result, last_colon_pos + 2);
  }
  return result;
}

internal E_KFTVSPropertiesAccel *
e_type_kft_vs_properties_accel_from_key(Arena *arena, E_TypeKey key)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  E_KFTVSPropertiesAccel *accel = push_array(arena, E_KFTVSPropertiesAccel, 1);
  {
    E_Type *type = e_type_from_key(key);
    E_KFTVSPropertyItem *items = push_array(scratch.arena, E_KFTVSPropertyItem, type->count);
    
    // KFT: match Visual Studio's shape: parent-class scopes first, then direct fields.
    for(U64 pass = 0; pass < 2; pass += 1)
    {
      for(U64 member_idx = 0; member_idx < type->count; member_idx += 1)
      {
        E_Member *member = &type->members[member_idx];
        B32 is_field = (member->name.size != 0 &&
                        member->kind == E_MemberKind_DataField &&
                        !e_type_kft_member_name_is_padding_noise(member->name));
        B32 is_base = (member->kind == E_MemberKind_Base ||
                       member->kind == E_MemberKind_VirtualBase);
        if((pass == 0 && is_base) || (pass == 1 && is_field))
        {
          E_KFTVSPropertyItem *item = &items[accel->count];
          item->kind = is_base ? E_KFTVSPropertyItemKind_Base : E_KFTVSPropertyItemKind_Field;
          item->off = member->off;
          item->type_key = member->type_key;
          if(is_base)
          {
            item->name = e_type_kft_simple_type_name_from_key(arena, member->type_key);
          }
          else
          {
            item->name = push_str8_copy(arena, member->name);
          }
          item->id = e_type_kft_vs_property_item_id(item->kind, item->name, item->off, item->type_key);
          accel->count += 1;
        }
      }
    }
    
    accel->items = push_array(arena, E_KFTVSPropertyItem, accel->count);
    for(U64 idx = 0; idx < accel->count; idx += 1)
    {
      MemoryCopyStruct(&accel->items[idx], &items[idx]);
    }
  }
  
  scratch_end(scratch);
  return accel;
}

internal E_Eval
e_type_kft_eval_from_vs_property_item(Arena *arena, E_Eval parent_eval, E_KFTVSPropertyItem *item)
{
  if(item->kind == E_KFTVSPropertyItemKind_Field)
  {
    E_Eval result = e_eval_wrapf(parent_eval, "$.%S", item->name);
    return result;
  }
  
  E_Eval result = parent_eval;
  U64 vaddr = parent_eval.value.u64 + item->off;
  String8 key_string = push_str8f(arena, "kft_vs_%I64x_%S", item->id, item->name);
  
  result.key = e_key_wrap(parent_eval.key, key_string);
  result.parent_key = parent_eval.key;
  result.string = push_str8f(arena, item->kind == E_KFTVSPropertyItemKind_Base ? "%S" : ".%S", item->name);
  result.expr = &e_expr_nil;
  result.irtree.root = e_irtree_const_u(arena, vaddr);
  result.irtree.type_key = item->type_key;
  result.irtree.mode = E_Mode_Offset;
  result.irtree.msgs = (E_MsgList){0};
  result.irtree.prev = 0;
  result.irtree.auto_hook = 0;
  if(item->kind == E_KFTVSPropertyItemKind_Base)
  {
    result.irtree.type_key = e_type_key_cons_meta_display_name(item->type_key, item->name);
  }
  result.bytecode = str8_zero();
  result.code = E_InterpretationCode_Good;
  result.value = e_value_u64(vaddr);
  result.msgs = (E_MsgList){0};
  return result;
}

E_TYPE_EXPAND_INFO_FUNCTION_DEF(kft_vs_properties)
{
  E_TypeExpandInfo result = {0};
  E_TypeKey expand_type_key = e_default_expansion_type_from_key(eval.irtree.type_key);
  E_TypeKind expand_type_kind = e_type_kind_from_key(expand_type_key);
  if(expand_type_kind == E_TypeKind_Struct ||
     expand_type_kind == E_TypeKind_Class ||
     expand_type_kind == E_TypeKind_Union)
  {
    E_KFTVSPropertiesAccel *accel = e_type_kft_vs_properties_accel_from_key(arena, expand_type_key);
    result.user_data = accel;
    result.expr_count = accel->count;
  }
  else
  {
    result = E_TYPE_EXPAND_INFO_FUNCTION_NAME(default)(arena, eval, filter);
  }
  return result;
}

E_TYPE_EXPAND_RANGE_FUNCTION_DEF(kft_vs_properties)
{
  E_TypeKey expand_type_key = e_default_expansion_type_from_key(eval.irtree.type_key);
  E_TypeKind expand_type_kind = e_type_kind_from_key(expand_type_key);
  if(expand_type_kind == E_TypeKind_Struct ||
     expand_type_kind == E_TypeKind_Class ||
     expand_type_kind == E_TypeKind_Union)
  {
    E_KFTVSPropertiesAccel *accel = (E_KFTVSPropertiesAccel *)user_data;
    if(accel == 0)
    {
      accel = e_type_kft_vs_properties_accel_from_key(arena, expand_type_key);
    }
    Rng1U64 legal_idx_range = r1u64(0, accel->count);
    Rng1U64 read_range = intersect_1u64(legal_idx_range, idx_range);
    U64 read_range_count = dim_1u64(read_range);
    for(U64 idx = 0; idx < read_range_count; idx += 1)
    {
      U64 item_idx = idx + read_range.min;
      evals_out[idx] = e_type_kft_eval_from_vs_property_item(arena, eval, &accel->items[item_idx]);
    }
  }
  else
  {
    E_TYPE_EXPAND_RANGE_FUNCTION_NAME(default)(arena, user_data, eval, filter, idx_range, evals_out);
  }
}

E_TYPE_EXPAND_ID_FROM_NUM_FUNCTION_DEF(kft_vs_properties)
{
  U64 result = num;
  E_KFTVSPropertiesAccel *accel = (E_KFTVSPropertiesAccel *)user_data;
  if(accel != 0 && 0 < num && num <= accel->count)
  {
    result = accel->items[num-1].id;
  }
  return result;
}

E_TYPE_EXPAND_NUM_FROM_ID_FUNCTION_DEF(kft_vs_properties)
{
  U64 result = id;
  E_KFTVSPropertiesAccel *accel = (E_KFTVSPropertiesAccel *)user_data;
  if(accel != 0)
  {
    result = 0;
    for(U64 idx = 0; idx < accel->count; idx += 1)
    {
      if(accel->items[idx].id == id)
      {
        result = idx + 1;
        break;
      }
    }
  }
  return result;
}

global read_only E_TypeExpandRule e_type_expand_rule__kft_vs_properties =
{
  E_TYPE_EXPAND_INFO_FUNCTION_NAME(kft_vs_properties),
  E_TYPE_EXPAND_RANGE_FUNCTION_NAME(kft_vs_properties),
  E_TYPE_EXPAND_ID_FROM_NUM_FUNCTION_NAME(kft_vs_properties),
  E_TYPE_EXPAND_NUM_FROM_ID_FUNCTION_NAME(kft_vs_properties),
};
