// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_EVAL_H
#define NATVIS_EVAL_H

////////////////////////////////
//~ NatVis Expression Translation

// Translates a NatVis expression string into a RAD eval-compatible expression.
// Handles: $T1→template_args[0], (Type*)→cast(Type *), this→$, $i→index_var
internal String8 nv_translate_expr(Arena *arena, String8 natvis_expr, String8 *template_args, U64 template_arg_count);

// Translates format specifier to a lens wrapper
// e.g. ",x" → wraps expr in "hex(...)", ",d" → identity
internal String8 nv_apply_format_spec(Arena *arena, String8 expr, String8 format_spec);

////////////////////////////////
//~ NatVis DisplayString Rendering (to a flat string with {expr} replaced by placeholders)

// Produces a RAD eval expression string that represents the DisplayString.
// For types with only a DisplayString (no Expand), this is used as the auto_hook tag expr.
internal String8 nv_display_string_to_tag_expr(Arena *arena, NV_DisplayString *ds, String8 *template_args, U64 template_arg_count);

////////////////////////////////
//~ NatVis → type_view expression generation

// For a TypeDef, produce the best auto_hook tag expression.
// Uses DisplayString first part's expression, or first Item, etc.
internal String8 nv_type_view_expr_from_typedef(Arena *arena, NV_TypeDef *td, String8 *template_args, U64 template_arg_count);

#endif // NATVIS_EVAL_H
