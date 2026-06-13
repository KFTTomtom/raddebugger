// KFT-specific eval visualization hooks. Kept outside upstream RAD files to
// minimize sync friction with the eval_visualization layer.

#ifndef KFT_EVAL_VISUALIZATION_EXTENSION_H
#define KFT_EVAL_VISUALIZATION_EXTENSION_H

internal B32 kft_ev_type_key_matches_unreal_object_name(E_TypeKey type_key);
internal B32 kft_ev_type_key_is_or_inherits_from_unreal_object(E_TypeKey type_key, U32 max_depth);
internal B32 kft_ev_type_key_is_unreal_object_pointer_or_ref(E_TypeKey type_key);

internal B32 kft_ev_pointer_should_emit_address_first(EV_StringParams *params, E_TypeKind type_kind);
internal B32 kft_ev_pointer_should_descend_inline(E_TypeKind type_kind);

internal String8 kft_ev_string_from_auto_hook_summary(Arena *arena, EV_StringParams *params, E_Eval eval, String8 summary_string);
internal String8 kft_ev_string_from_default_struct_summary(Arena *arena, EV_StringParams *params, E_Eval eval);
internal String8 kft_ev_string_from_pointer_properties(Arena *arena, EV_StringParams *params, E_Eval eval, String8 ptr_value_string, B32 is_unreal_object_ptr_or_ref);

#endif // KFT_EVAL_VISUALIZATION_EXTENSION_H
