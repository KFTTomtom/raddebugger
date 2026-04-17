// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis Expansion — produce output items from an Expand block

internal NV_OutputItemList
nv_expand_items_from_expand(Arena *arena, NV_Expand *expand, String8 *template_args, U64 template_arg_count)
{
  NV_OutputItemList out = {0};
  if(expand == 0) { return out; }
  
  for(NV_ExpandItem *ei = expand->first_item; ei != 0; ei = ei->next)
  {
    switch(ei->kind)
    {
      case NV_ExpandItemKind_Item:
      {
        NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
        oi->name = ei->item.name;
        oi->expression = nv_translate_expr(arena, ei->item.expression, template_args, template_arg_count);
        SLLQueuePush(out.first, out.last, oi);
        out.count += 1;
      } break;
      
      case NV_ExpandItemKind_ArrayItems:
      {
        // emit a single synthetic item that uses the array() lens
        String8 size_e = nv_translate_expr(arena, ei->array_items.size_expr, template_args, template_arg_count);
        String8 ptr_e = nv_translate_expr(arena, ei->array_items.value_pointer_expr, template_args, template_arg_count);
        if(size_e.size > 0 && ptr_e.size > 0)
        {
          NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
          oi->name = str8_lit("[elements]");
          oi->expression = push_str8f(arena, "array(%S, %S)", ptr_e, size_e);
          SLLQueuePush(out.first, out.last, oi);
          out.count += 1;
        }
      } break;
      
      case NV_ExpandItemKind_IndexListItems:
      {
        // can't fully expand without runtime eval; emit a placeholder expression
        String8 size_e = nv_translate_expr(arena, ei->index_list_items.size_expr, template_args, template_arg_count);
        String8 val_e = nv_translate_expr(arena, ei->index_list_items.value_node_expr, template_args, template_arg_count);
        NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
        oi->name = str8_lit("[indexed]");
        oi->expression = push_str8f(arena, "/* IndexListItems: size=%S, node=%S */", size_e, val_e);
        SLLQueuePush(out.first, out.last, oi);
        out.count += 1;
      } break;
      
      case NV_ExpandItemKind_LinkedListItems:
      {
        String8 head_e = nv_translate_expr(arena, ei->linked_list_items.head_pointer_expr, template_args, template_arg_count);
        if(head_e.size > 0)
        {
          NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
          oi->name = str8_lit("[list]");
          oi->expression = push_str8f(arena, "list(%S)", head_e);
          SLLQueuePush(out.first, out.last, oi);
          out.count += 1;
        }
      } break;
      
      case NV_ExpandItemKind_Synthetic:
      {
        NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
        oi->name = ei->synthetic.name;
        if(ei->synthetic.first_display_string != 0)
        {
          oi->expression = nv_display_string_to_tag_expr(arena, ei->synthetic.first_display_string, template_args, template_arg_count);
        }
        else if(ei->synthetic.watch_expression.size > 0)
        {
          oi->expression = nv_translate_expr(arena, ei->synthetic.watch_expression, template_args, template_arg_count);
        }
        else
        {
          oi->expression = ei->synthetic.name;
        }
        SLLQueuePush(out.first, out.last, oi);
        out.count += 1;
        
        // recurse into nested expand
        if(ei->synthetic.expand != 0)
        {
          NV_OutputItemList sub = nv_expand_items_from_expand(arena, ei->synthetic.expand, template_args, template_arg_count);
          for(NV_OutputItem *si = sub.first; si != 0; )
          {
            NV_OutputItem *next = si->next;
            si->name = push_str8f(arena, "%S.%S", ei->synthetic.name, si->name);
            SLLQueuePush(out.first, out.last, si);
            out.count += 1;
            si = next;
          }
        }
      } break;
      
      case NV_ExpandItemKind_ExpandedItem:
      {
        NV_OutputItem *oi = push_array(arena, NV_OutputItem, 1);
        oi->name = str8_lit("[base]");
        oi->expression = nv_translate_expr(arena, ei->expanded_item.expression, template_args, template_arg_count);
        SLLQueuePush(out.first, out.last, oi);
        out.count += 1;
      } break;
      
      case NV_ExpandItemKind_CustomListItems:
      {
        NV_OutputItemList cl_out = nv_cl_interpret(arena, &ei->custom_list_items, template_args, template_arg_count);
        for(NV_OutputItem *ci = cl_out.first; ci != 0; )
        {
          NV_OutputItem *next = ci->next;
          ci->next = 0;
          SLLQueuePush(out.first, out.last, ci);
          out.count += 1;
          ci = next;
        }
      } break;
      
      default: break;
    }
  }
  
  return out;
}

////////////////////////////////
//~ NatVis CustomListItems Interpreter

internal NV_CLRuntimeVar *
nv_cl_find_var(NV_CLInterpreter *interp, String8 name)
{
  for(NV_CLRuntimeVar *v = interp->first_var; v != 0; v = v->next)
  {
    if(str8_match(v->name, name, 0)) { return v; }
  }
  return 0;
}

internal String8
nv_cl_substitute_vars(Arena *arena, NV_CLInterpreter *interp, String8 expr)
{
  // substitute variable names in expression text
  // this is a textual substitution (same as NatVis semantics)
  String8List parts = {0};
  U64 i = 0;
  B32 any_sub = 0;
  
  while(i < expr.size)
  {
    B32 found_var = 0;
    for(NV_CLRuntimeVar *v = interp->first_var; v != 0; v = v->next)
    {
      if(i + v->name.size <= expr.size)
      {
        String8 window = str8(expr.str + i, v->name.size);
        if(str8_match(window, v->name, 0))
        {
          // check that it's not part of a larger identifier
          B32 left_ok = (i == 0);
          if(!left_ok)
          {
            U8 lc = expr.str[i-1];
            left_ok = !((lc >= 'a' && lc <= 'z') || (lc >= 'A' && lc <= 'Z') || (lc >= '0' && lc <= '9') || lc == '_');
          }
          B32 right_ok = (i + v->name.size >= expr.size);
          if(!right_ok)
          {
            U8 rc = expr.str[i + v->name.size];
            right_ok = !((rc >= 'a' && rc <= 'z') || (rc >= 'A' && rc <= 'Z') || (rc >= '0' && rc <= '9') || rc == '_');
          }
          
          if(left_ok && right_ok)
          {
            str8_list_push(arena, &parts, str8_lit("("));
            str8_list_push(arena, &parts, v->value_expr);
            str8_list_push(arena, &parts, str8_lit(")"));
            i += v->name.size;
            found_var = 1;
            any_sub = 1;
            break;
          }
        }
      }
    }
    
    if(!found_var)
    {
      str8_list_push(arena, &parts, str8(expr.str + i, 1));
      i += 1;
    }
  }
  
  if(!any_sub) { return expr; }
  return str8_list_join(arena, &parts, 0);
}

internal void
nv_cl_execute_statements(NV_CLInterpreter *interp, NV_CLStatement *first)
{
  for(NV_CLStatement *stmt = first; stmt != 0 && !interp->break_requested; stmt = stmt->next)
  {
    interp->total_iterations += 1;
    if(interp->total_iterations > NV_CL_MAX_ITERATIONS)
    {
      interp->break_requested = 1;
      return;
    }
    
    switch(stmt->kind)
    {
      case NV_CLStatementKind_Loop:
      {
        // loop until Break or max iterations
        while(!interp->break_requested)
        {
          interp->total_iterations += 1;
          if(interp->total_iterations > NV_CL_MAX_ITERATIONS)
          {
            interp->break_requested = 1;
            break;
          }
          
          // save break state for inner breaks
          B32 saved_break = interp->break_requested;
          nv_cl_execute_statements(interp, stmt->first_child);
          
          // a break inside the loop breaks the loop
          if(interp->break_requested)
          {
            interp->break_requested = saved_break;
            break;
          }
        }
      } break;
      
      case NV_CLStatementKind_If:
      {
        // for static analysis (no runtime eval), always execute If body
        // when runtime eval is available, check stmt->condition
        nv_cl_execute_statements(interp, stmt->first_child);
        
        // skip following Elseif/Else since we executed this branch
        while(stmt->next != 0 &&
              (stmt->next->kind == NV_CLStatementKind_Elseif ||
               stmt->next->kind == NV_CLStatementKind_Else))
        {
          stmt = stmt->next;
        }
      } break;
      
      case NV_CLStatementKind_Elseif:
      case NV_CLStatementKind_Else:
      {
        // only reached if previous If/Elseif was not taken
        // (handled by If case above skipping these)
      } break;
      
      case NV_CLStatementKind_Exec:
      {
        // parse "var = expr" or "var op= expr"
        String8 exec_expr = stmt->expression;
        
        // find '=' not preceded by !, <, >, = and not followed by =
        U64 eq_pos = exec_expr.size;
        for(U64 j = 0; j < exec_expr.size; j += 1)
        {
          if(exec_expr.str[j] == '=')
          {
            B32 is_compare = 0;
            if(j > 0)
            {
              U8 prev = exec_expr.str[j-1];
              if(prev == '!' || prev == '<' || prev == '>' || prev == '=') { is_compare = 1; }
              if(prev == '+' || prev == '-' || prev == '*' || prev == '/' || prev == '|' || prev == '&' || prev == '^')
              {
                // compound assignment (+=, -=, etc.)
                eq_pos = j - 1;
                break;
              }
            }
            if(j + 1 < exec_expr.size && exec_expr.str[j+1] == '=') { is_compare = 1; }
            if(!is_compare)
            {
              eq_pos = j;
              break;
            }
          }
        }
        
        if(eq_pos < exec_expr.size)
        {
          String8 var_name = str8_skip_chop_whitespace(str8(exec_expr.str, eq_pos));
          // find the actual '=' for the RHS
          U64 rhs_start = eq_pos;
          while(rhs_start < exec_expr.size && exec_expr.str[rhs_start] != '=') { rhs_start += 1; }
          rhs_start += 1;
          String8 rhs = str8_skip_chop_whitespace(str8(exec_expr.str + rhs_start, exec_expr.size - rhs_start));
          
          String8 translated_rhs = nv_cl_substitute_vars(interp->arena, interp, rhs);
          translated_rhs = nv_translate_expr(interp->arena, translated_rhs, interp->template_args, interp->template_arg_count);
          
          NV_CLRuntimeVar *var = nv_cl_find_var(interp, var_name);
          if(var != 0)
          {
            var->value_expr = translated_rhs;
          }
        }
      } break;
      
      case NV_CLStatementKind_Break:
      {
        if(stmt->condition.size == 0)
        {
          interp->break_requested = 1;
        }
        else
        {
          // conditional break: in static analysis mode (no runtime eval),
          // only break after we've produced at least one item to prove
          // the structure is correct
          if(interp->output.count > 0)
          {
            interp->break_requested = 1;
          }
        }
      } break;
      
      case NV_CLStatementKind_Item:
      {
        if(interp->output.count >= interp->max_items)
        {
          interp->break_requested = 1;
          return;
        }
        
        String8 expr = nv_cl_substitute_vars(interp->arena, interp, stmt->expression);
        expr = nv_translate_expr(interp->arena, expr, interp->template_args, interp->template_arg_count);
        
        NV_OutputItem *oi = push_array(interp->arena, NV_OutputItem, 1);
        if(stmt->item_name.size > 0)
        {
          oi->name = stmt->item_name;
        }
        else
        {
          oi->name = push_str8f(interp->arena, "[%llu]", interp->output.count);
        }
        oi->expression = expr;
        SLLQueuePush(interp->output.first, interp->output.last, oi);
        interp->output.count += 1;
      } break;
      
      default: break;
    }
  }
}

internal NV_OutputItemList
nv_cl_interpret(Arena *arena, NV_CustomListItemsData *data, String8 *template_args, U64 template_arg_count)
{
  NV_CLInterpreter interp = {0};
  interp.arena = arena;
  interp.max_items = data->max_items_per_view;
  interp.template_args = template_args;
  interp.template_arg_count = template_arg_count;
  
  // init variables
  for(NV_CLVariable *v = data->first_variable; v != 0; v = v->next)
  {
    NV_CLRuntimeVar *rv = push_array(arena, NV_CLRuntimeVar, 1);
    rv->name = v->name;
    rv->value_expr = nv_translate_expr(arena, v->initial_value, template_args, template_arg_count);
    SLLQueuePush(interp.first_var, interp.last_var, rv);
  }
  
  // execute statement tree
  nv_cl_execute_statements(&interp, data->first_statement);
  
  return interp.output;
}
