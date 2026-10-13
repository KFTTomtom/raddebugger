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
kft_ev_type_key_matches_ffloat16_name(E_TypeKey type_key)
{
  B32 result = 0;
  if(!e_type_key_match(type_key, e_type_key_zero()))
  {
    E_Type *type = e_type_from_key(type_key);
    if(type != 0 && type != &e_type_nil && type->kind != E_TypeKind_Null)
    {
      Temp scratch = scratch_begin(0, 0);
      String8 type_name = e_type_string_from_key(scratch.arena, type_key);
      result = str8_match(type_name, str8_lit("FFloat16"), 0);
      if(!result && type_name.size > str8_lit("::FFloat16").size)
      {
        result = str8_match(str8_skip(type_name, type_name.size - str8_lit("::FFloat16").size), str8_lit("::FFloat16"), 0);
      }
      scratch_end(scratch);
    }
  }
  return result;
}

internal B32
kft_ev_type_key_should_skip_default_struct_summary(E_TypeKey type_key)
{
  B32 result = 0;
  if(!e_type_key_match(type_key, e_type_key_zero()))
  {
    E_Type *type = e_type_from_key(type_key);
    if(type != 0 && type != &e_type_nil && type->kind != E_TypeKind_Null)
    {
      Temp scratch = scratch_begin(0, 0);
      String8 type_name = e_type_string_from_key(scratch.arena, type_key);
      result = (str8_find_needle(type_name, 0, str8_lit("UE::Math::TVector<"), 0) < type_name.size ||
                str8_find_needle(type_name, 0, str8_lit("UE::Math::TTransform<"), 0) < type_name.size ||
                str8_find_needle(type_name, 0, str8_lit("VectorRegister"), 0) < type_name.size);
      scratch_end(scratch);
    }
  }
  return result;
}

internal B32
kft_ev_type_key_name_contains(E_TypeKey type_key, String8 needle)
{
  B32 result = 0;
  if(!e_type_key_match(type_key, e_type_key_zero()))
  {
    E_Type *type = e_type_from_key(type_key);
    if(type != 0 && type != &e_type_nil && type->kind != E_TypeKind_Null)
    {
      Temp scratch = scratch_begin(0, 0);
      String8 type_name = e_type_string_from_key(scratch.arena, type_key);
      result = (str8_find_needle(type_name, 0, needle, 0) < type_name.size);
      scratch_end(scratch);
    }
  }
  return result;
}

internal F32
kft_ev_f32_from_f16_bits(U16 bits)
{
  U32 sign = ((U32)bits & 0x8000u) << 16;
  S32 exponent = (S32)((bits >> 10) & 0x1fu);
  U32 mantissa = (U32)bits & 0x03ffu;
  U32 f32_bits = sign;
  
  if(exponent == 0)
  {
    if(mantissa != 0)
    {
      for(; (mantissa & 0x0400u) == 0; mantissa <<= 1)
      {
        exponent -= 1;
      }
      exponent += 1;
      mantissa &= 0x03ffu;
      f32_bits |= (U32)(exponent + 127 - 15) << 23;
      f32_bits |= mantissa << 13;
    }
  }
  else if(exponent == 31)
  {
    f32_bits |= 0xffu << 23;
    f32_bits |= mantissa << 13;
  }
  else
  {
    f32_bits |= (U32)(exponent + 127 - 15) << 23;
    f32_bits |= mantissa << 13;
  }
  
  union { U32 u; F32 f; } converter;
  converter.u = f32_bits;
  return converter.f;
}

internal B32 kft_ev_bytes_from_eval_byte_offset(E_Eval eval, U64 byte_off, void *out, U64 size);

internal B32
kft_ev_encoded_from_ffloat16_eval(E_Eval eval, U16 *encoded_out)
{
  B32 result = 0;
  E_Eval value_eval = e_value_eval_from_eval(eval);
  if(value_eval.irtree.mode == E_Mode_Value)
  {
    *encoded_out = (U16)value_eval.value.u64;
    result = 1;
  }
  else
  {
    E_Eval encoded_eval = e_eval_wrapf(eval, "$.Encoded");
    if(encoded_eval.msgs.max_kind == E_MsgKind_Null)
    {
      E_Eval encoded_value_eval = e_value_eval_from_eval(encoded_eval);
      if(encoded_value_eval.irtree.mode == E_Mode_Value)
      {
        *encoded_out = (U16)encoded_value_eval.value.u64;
        result = 1;
      }
    }
  }
  return result;
}

internal B32
kft_ev_encoded_from_eval_byte_offset(E_Eval eval, U64 byte_off, U16 *encoded_out)
{
  B32 result = 0;
  result = kft_ev_bytes_from_eval_byte_offset(eval, byte_off, encoded_out, sizeof(*encoded_out));
  return result;
}

internal B32
kft_ev_bytes_from_eval_byte_offset(E_Eval eval, U64 byte_off, void *out, U64 size)
{
  B32 result = 0;
  E_TypeKey type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  U64 type_byte_size = e_type_byte_size_from_key(type_key);
  E_Eval value_eval = e_value_eval_from_eval(eval);
  if(value_eval.irtree.mode == E_Mode_Value &&
     byte_off + size <= type_byte_size &&
     type_byte_size <= sizeof(E_Value))
  {
    MemoryCopy(out, value_eval.value.u512.u8 + byte_off, size);
    result = 1;
  }
  else if(eval.irtree.mode == E_Mode_Offset)
  {
    U64 vaddr = eval.value.u64 + byte_off;
    result = e_space_read(eval.space, out, 0, r1u64(vaddr, vaddr + size));
  }
  return result;
}

internal B32
kft_ev_string_is_simple_member_name(String8 string)
{
  B32 result = string.size != 0;
  for(U64 idx = 0; idx < string.size && result; idx += 1)
  {
    U8 byte = string.str[idx];
    result = (char_is_alpha(byte) || char_is_digit(byte, 10) || byte == '_');
  }
  return result;
}

internal String8
kft_ev_string_from_ffloat16_member(Arena *arena, EV_StringParams *params, E_Eval parent_eval, String8 member_name)
{
  String8 result = {0};
  if(kft_ev_string_is_simple_member_name(member_name))
  {
    E_TypeKey expand_type_key = e_default_expansion_type_from_key(parent_eval.irtree.type_key);
    E_MemberArray data_members = e_type_data_members_from_key_filter__cached(expand_type_key, params->filter);
    for(U64 member_idx = 0; member_idx < data_members.count; member_idx += 1)
    {
      E_Member *member = &data_members.v[member_idx];
      E_TypeKey member_type_key = e_type_key_unwrap(member->type_key, E_TypeUnwrapFlag_AllDecorative);
      if(str8_match(member->name, member_name, 0) &&
         kft_ev_type_key_matches_ffloat16_name(member_type_key))
      {
        U16 encoded = 0;
        if(kft_ev_encoded_from_eval_byte_offset(parent_eval, member->off, &encoded))
        {
          E_Eval f32_eval = e_value_eval_from_eval(parent_eval);
          f32_eval.irtree.type_key = e_type_key_basic(E_TypeKind_F32);
          f32_eval.irtree.mode = E_Mode_Value;
          f32_eval.value.f32 = kft_ev_f32_from_f16_bits(encoded);
          result = ev_string_from_simple_typed_eval(arena, params, f32_eval);
        }
        break;
      }
    }
  }
  return result;
}

internal String8
kft_ev_string_from_f32_value(Arena *arena, EV_StringParams *params, F32 value)
{
  E_Eval f32_eval = e_eval_nil;
  f32_eval.irtree.type_key = e_type_key_basic(E_TypeKind_F32);
  f32_eval.irtree.mode = E_Mode_Value;
  f32_eval.value.f32 = value;
  String8 result = ev_string_from_simple_typed_eval(arena, params, f32_eval);
  return result;
}

internal String8
kft_ev_string_from_f64_value(Arena *arena, EV_StringParams *params, F64 value)
{
  E_Eval f64_eval = e_eval_nil;
  f64_eval.irtree.type_key = e_type_key_basic(E_TypeKind_F64);
  f64_eval.irtree.mode = E_Mode_Value;
  f64_eval.value.f64 = value;
  String8 result = ev_string_from_simple_typed_eval(arena, params, f64_eval);
  return result;
}

internal String8
kft_ev_string_from_basic_member(Arena *arena, EV_StringParams *params, E_Eval parent_eval, E_Member *member)
{
  String8 result = {0};
  E_TypeKey member_type_key = e_type_key_unwrap(member->type_key, E_TypeUnwrapFlag_AllDecorative);
  E_TypeKind member_type_kind = e_type_kind_from_key(member_type_key);
  if(member_type_kind == E_TypeKind_F32)
  {
    F32 value = 0;
    if(kft_ev_bytes_from_eval_byte_offset(parent_eval, member->off, &value, sizeof(value)))
    {
      result = kft_ev_string_from_f32_value(arena, params, value);
    }
  }
  else if(member_type_kind == E_TypeKind_F64)
  {
    F64 value = 0;
    if(kft_ev_bytes_from_eval_byte_offset(parent_eval, member->off, &value, sizeof(value)))
    {
      result = kft_ev_string_from_f64_value(arena, params, value);
    }
  }
  else if(kft_ev_type_key_matches_ffloat16_name(member_type_key))
  {
    result = kft_ev_string_from_ffloat16_member(arena, params, parent_eval, member->name);
  }
  return result;
}

internal E_Member *
kft_ev_member_from_name(E_MemberArray members, String8 name)
{
  E_Member *result = 0;
  for(U64 member_idx = 0; member_idx < members.count; member_idx += 1)
  {
    E_Member *member = &members.v[member_idx];
    if(str8_match(member->name, name, 0))
    {
      result = member;
      break;
    }
  }
  return result;
}

internal String8
kft_ev_string_from_tvector(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  E_TypeKey expand_type_key = e_default_expansion_type_from_key(eval.irtree.type_key);
  E_MemberArray members = e_type_data_members_from_key_filter__cached(expand_type_key, params->filter);
  E_Member *x = kft_ev_member_from_name(members, str8_lit("X"));
  E_Member *y = kft_ev_member_from_name(members, str8_lit("Y"));
  E_Member *z = kft_ev_member_from_name(members, str8_lit("Z"));
  if(x != 0 && y != 0 && z != 0)
  {
    String8 x_string = kft_ev_string_from_basic_member(scratch.arena, params, eval, x);
    String8 y_string = kft_ev_string_from_basic_member(scratch.arena, params, eval, y);
    String8 z_string = kft_ev_string_from_basic_member(scratch.arena, params, eval, z);
    if(x_string.size != 0 && y_string.size != 0 && z_string.size != 0)
    {
      result = push_str8f(arena, "{X=%S, Y=%S, Z=%S}", x_string, y_string, z_string);
    }
  }
  scratch_end(scratch);
  return result;
}

internal String8
kft_ev_string_from_vector_register_at_offset(Arena *arena, EV_StringParams *params, E_Eval eval, U64 byte_off, E_TypeKey type_key)
{
  String8 result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  String8 type_name = e_type_string_from_key(scratch.arena, e_type_key_unwrap(type_key, E_TypeUnwrapFlag_AllDecorative));
  if(str8_find_needle(type_name, 0, str8_lit("VectorRegister4Double"), 0) < type_name.size ||
     str8_find_needle(type_name, 0, str8_lit("PersistentVectorRegister4Double"), 0) < type_name.size)
  {
    F64 values[4] = {0};
    if(kft_ev_bytes_from_eval_byte_offset(eval, byte_off, values, sizeof(values)))
    {
      String8 x = kft_ev_string_from_f64_value(scratch.arena, params, values[0]);
      String8 y = kft_ev_string_from_f64_value(scratch.arena, params, values[1]);
      String8 z = kft_ev_string_from_f64_value(scratch.arena, params, values[2]);
      String8 w = kft_ev_string_from_f64_value(scratch.arena, params, values[3]);
      result = push_str8f(arena, "{{%S, %S}, {%S, %S}}", x, y, z, w);
    }
  }
  else if(str8_find_needle(type_name, 0, str8_lit("VectorRegister4Float"), 0) < type_name.size ||
          str8_find_needle(type_name, 0, str8_lit("PersistentVectorRegister4Float"), 0) < type_name.size ||
          str8_match(type_name, str8_lit("__m128"), 0))
  {
    F32 values[4] = {0};
    if(kft_ev_bytes_from_eval_byte_offset(eval, byte_off, values, sizeof(values)))
    {
      String8 x = kft_ev_string_from_f32_value(scratch.arena, params, values[0]);
      String8 y = kft_ev_string_from_f32_value(scratch.arena, params, values[1]);
      String8 z = kft_ev_string_from_f32_value(scratch.arena, params, values[2]);
      String8 w = kft_ev_string_from_f32_value(scratch.arena, params, values[3]);
      result = push_str8f(arena, "{%S, %S, %S, %S}", x, y, z, w);
    }
  }
  scratch_end(scratch);
  return result;
}

internal String8
kft_ev_string_from_vector_register(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = kft_ev_string_from_vector_register_at_offset(arena, params, eval, 0, eval.irtree.type_key);
  return result;
}

internal String8
kft_ev_string_from_ttransform(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  E_TypeKey expand_type_key = e_default_expansion_type_from_key(eval.irtree.type_key);
  E_MemberArray members = e_type_data_members_from_key_filter__cached(expand_type_key, params->filter);
  E_Member *rotation = kft_ev_member_from_name(members, str8_lit("Rotation"));
  E_Member *translation = kft_ev_member_from_name(members, str8_lit("Translation"));
  E_Member *scale3d = kft_ev_member_from_name(members, str8_lit("Scale3D"));
  if(rotation != 0 && translation != 0 && scale3d != 0)
  {
    String8 rotation_string = kft_ev_string_from_vector_register_at_offset(scratch.arena, params, eval, rotation->off, rotation->type_key);
    String8 translation_string = kft_ev_string_from_vector_register_at_offset(scratch.arena, params, eval, translation->off, translation->type_key);
    String8 scale3d_string = kft_ev_string_from_vector_register_at_offset(scratch.arena, params, eval, scale3d->off, scale3d->type_key);
    if(rotation_string.size != 0 && translation_string.size != 0 && scale3d_string.size != 0)
    {
      result = push_str8f(arena, "{Rotation=%S, Translation=%S, Scale3D=%S}", rotation_string, translation_string, scale3d_string);
    }
  }
  scratch_end(scratch);
  return result;
}

internal String8
kft_ev_string_from_ue_math_type(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  E_TypeKey original_type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  if(kft_ev_type_key_name_contains(original_type_key, str8_lit("UE::Math::TVector<")))
  {
    result = kft_ev_string_from_tvector(arena, params, eval);
  }
  else if(kft_ev_type_key_name_contains(original_type_key, str8_lit("UE::Math::TTransform<")))
  {
    result = kft_ev_string_from_ttransform(arena, params, eval);
  }
  else if(kft_ev_type_key_name_contains(original_type_key, str8_lit("VectorRegister")))
  {
    result = kft_ev_string_from_vector_register(arena, params, eval);
  }
  return result;
}

internal B32
kft_ev_type_kind_is_text_code_unit(E_TypeKind type_kind)
{
  B32 result = ((E_TypeKind_Char8 <= type_kind && type_kind <= E_TypeKind_UChar32) ||
                type_kind == E_TypeKind_S8 ||
                type_kind == E_TypeKind_U8);
  return result;
}

internal B32
kft_ev_type_key_is_text_code_unit(E_TypeKey type_key)
{
  E_TypeKey unwrapped_type_key = e_type_key_unwrap(type_key, E_TypeUnwrapFlag_AllDecorative);
  E_TypeKind type_kind = e_type_kind_from_key(unwrapped_type_key);
  B32 result = kft_ev_type_kind_is_text_code_unit(type_kind);
  return result;
}

internal B32
kft_ev_type_key_is_text_storage(E_TypeKey type_key)
{
  B32 result = 0;
  E_TypeKey unwrapped_type_key = e_type_key_unwrap(type_key, E_TypeUnwrapFlag_AllDecorative);
  E_TypeKind type_kind = e_type_kind_from_key(unwrapped_type_key);
  if(type_kind == E_TypeKind_Array ||
     type_kind == E_TypeKind_Ptr ||
     type_kind == E_TypeKind_LRef ||
     type_kind == E_TypeKind_RRef)
  {
    E_TypeKey direct_type_key = e_type_key_direct(unwrapped_type_key);
    result = kft_ev_type_key_is_text_code_unit(direct_type_key);
  }
  return result;
}

internal String8
kft_ev_string_from_text_storage(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  E_TypeKey type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  if((params->flags & EV_StringFlag_ReadOnlyDisplayRules) &&
     kft_ev_type_key_is_text_storage(type_key))
  {
    EV_StringParams value_params = *params;
    value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
    value_params.flags |= EV_StringFlag_DisableAddresses;
    result = ev_value_string_from_eval(arena, &value_params, eval, 256);
  }
  return result;
}

internal B32
kft_ev_pointer_should_emit_address_first(EV_StringParams *params, E_TypeKey type_key, E_TypeKind type_kind)
{
  B32 result = ((params->flags & EV_StringFlag_ReadOnlyDisplayRules) &&
                !kft_ev_type_key_is_text_storage(type_key) &&
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

internal String8
kft_ev_string_from_scalar_ref(Arena *arena, EV_StringParams *params, E_Eval eval, E_TypeKey type_key, E_TypeKind type_kind)
{
  String8 result = {0};
  if((params->flags & EV_StringFlag_ReadOnlyDisplayRules) &&
     (type_kind == E_TypeKind_LRef ||
      type_kind == E_TypeKind_RRef))
  {
    E_TypeKey direct_type_key = e_type_key_unwrap(e_type_key_direct(type_key), E_TypeUnwrapFlag_AllDecorative);
    E_TypeKind direct_type_kind = e_type_kind_from_key(direct_type_key);
    if(e_type_kind_is_basic_or_enum(direct_type_kind))
    {
      EV_StringParams value_params = *params;
      value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
      E_Eval deref_eval = e_eval_wrapf(eval, "*$");
      if(deref_eval.msgs.max_kind == E_MsgKind_Null)
      {
        result = ev_value_string_from_eval(arena, &value_params, deref_eval, 256);
      }
    }
  }
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
    String8 value_string = kft_ev_string_from_text_storage(scratch.arena, &value_params, summary_eval);
    if(value_string.size == 0)
    {
      value_string = kft_ev_string_from_ffloat16_member(scratch.arena, &value_params, eval, expr);
    }
    if(value_string.size == 0)
    {
      value_string = kft_ev_string_from_ffloat16(scratch.arena, &value_params, summary_eval);
    }
    if(value_string.size == 0)
    {
      value_string = ev_value_string_from_eval(scratch.arena, &value_params, summary_eval, 256);
    }
    if(value_string.size == 0 && expr.size != 0 && expr.str[0] != '$')
    {
      E_Eval member_summary_eval = e_eval_wrapf(eval, "$.%S", expr);
      value_string = kft_ev_string_from_text_storage(scratch.arena, &value_params, member_summary_eval);
      if(value_string.size == 0)
      {
        value_string = kft_ev_string_from_ffloat16(scratch.arena, &value_params, member_summary_eval);
      }
      if(value_string.size == 0)
      {
        value_string = ev_value_string_from_eval(scratch.arena, &value_params, member_summary_eval, 256);
      }
    }
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
kft_ev_string_from_ffloat16(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  E_TypeKey original_type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  if(kft_ev_type_key_matches_ffloat16_name(original_type_key))
  {
    U16 encoded = 0;
    if(kft_ev_encoded_from_ffloat16_eval(eval, &encoded))
    {
      E_Eval f32_eval = e_value_eval_from_eval(eval);
      f32_eval.irtree.type_key = e_type_key_basic(E_TypeKind_F32);
      f32_eval.irtree.mode = E_Mode_Value;
      f32_eval.value.f32 = kft_ev_f32_from_f16_bits(encoded);
      result = ev_string_from_simple_typed_eval(arena, params, f32_eval);
    }
  }
  return result;
}

internal String8
kft_ev_string_from_default_struct_summary(Arena *arena, EV_StringParams *params, E_Eval eval)
{
  String8 result = {0};
  E_TypeKey original_type_key = e_type_key_unwrap(eval.irtree.type_key, E_TypeUnwrapFlag_AllDecorative);
  E_TypeKind original_type_kind = e_type_kind_from_key(original_type_key);
  if(kft_ev_type_key_matches_ffloat16_name(original_type_key))
  {
    result = kft_ev_string_from_ffloat16(arena, params, eval);
    return result;
  }
  if(e_type_kft_show_properties_like_vs())
  {
    result = kft_ev_string_from_ue_math_type(arena, params, eval);
    if(result.size != 0)
    {
      return result;
    }
  }
  if(kft_ev_type_key_should_skip_default_struct_summary(original_type_key))
  {
    return result;
  }
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
        EV_StringParams value_params = *params;
        value_params.flags |= EV_StringFlag_DisableAutoHookSummaries;
        E_Eval member_eval = e_eval_wrapf(eval, "$.%S", member->name);
        String8 text_value_string = kft_ev_string_from_text_storage(scratch.arena, &value_params, member_eval);
        if(text_value_string.size != 0)
        {
          str8_list_push(scratch.arena, &strings, text_value_string);
        }
        else if(kft_ev_type_key_matches_ffloat16_name(unwrapped_type_key))
        {
          String8 value_string = kft_ev_string_from_ffloat16_member(scratch.arena, &value_params, eval, member->name);
          if(value_string.size == 0)
          {
            value_string = kft_ev_string_from_ffloat16(scratch.arena, &value_params, member_eval);
          }
          str8_list_push(scratch.arena, &strings, value_string);
        }
        else if(e_type_kind_is_basic_or_enum(member_type_kind) ||
                e_type_kind_is_pointer_or_ref(member_type_kind))
        {
          if(member_type_kind == E_TypeKind_LRef ||
             member_type_kind == E_TypeKind_RRef)
          {
            E_TypeKey direct_type_key = e_type_key_unwrap(e_type_key_direct(unwrapped_type_key), E_TypeUnwrapFlag_AllDecorative);
            E_TypeKind direct_type_kind = e_type_kind_from_key(direct_type_key);
            if(e_type_kind_is_basic_or_enum(direct_type_kind))
            {
              E_Eval deref_member_eval = e_eval_wrapf(eval, "*($.%S)", member->name);
              if(deref_member_eval.msgs.max_kind == E_MsgKind_Null)
              {
                member_eval = deref_member_eval;
              }
            }
          }
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
