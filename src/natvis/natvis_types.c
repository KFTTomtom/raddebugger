// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis DisplayString Parsing (split "{expr}" from literal text)

internal NV_DisplayString *
nv_display_string_from_xml(Arena *arena, NV_XMLNode *node)
{
  NV_DisplayString *ds = push_array(arena, NV_DisplayString, 1);
  ds->condition = nv_xml_attr_from_key(node, str8_lit("Condition"));
  ds->is_optional = nv_xml_attr_exists(node, str8_lit("Optional"));
  
  String8 raw = node->text;
  U64 i = 0;
  while(i < raw.size)
  {
    if(raw.str[i] == '{' && i + 1 < raw.size && raw.str[i+1] == '{')
    {
      // escaped brace {{ → literal {
      NV_DisplayPart *part = push_array(arena, NV_DisplayPart, 1);
      part->kind = NV_DisplayPartKind_Literal;
      part->text = str8_lit("{");
      SLLQueuePush(ds->first_part, ds->last_part, part);
      i += 2;
    }
    else if(raw.str[i] == '}' && i + 1 < raw.size && raw.str[i+1] == '}')
    {
      NV_DisplayPart *part = push_array(arena, NV_DisplayPart, 1);
      part->kind = NV_DisplayPartKind_Literal;
      part->text = str8_lit("}");
      SLLQueuePush(ds->first_part, ds->last_part, part);
      i += 2;
    }
    else if(raw.str[i] == '{')
    {
      // expression: find matching }
      i += 1;
      U64 expr_start = i;
      U64 depth = 1;
      while(i < raw.size && depth > 0)
      {
        if(raw.str[i] == '{') { depth += 1; }
        else if(raw.str[i] == '}') { depth -= 1; }
        if(depth > 0) { i += 1; }
      }
      String8 expr_text = str8(raw.str + expr_start, i - expr_start);
      if(i < raw.size) { i += 1; } // skip closing }
      
      // check for format specifier: ",x", ",d", etc. at end
      NV_DisplayPart *part = push_array(arena, NV_DisplayPart, 1);
      part->kind = NV_DisplayPartKind_Expression;
      
      // find last comma not inside parens/brackets for format spec
      U64 comma_pos = expr_text.size;
      U64 paren_depth = 0;
      for(U64 j = 0; j < expr_text.size; j += 1)
      {
        U8 c = expr_text.str[j];
        if(c == '(' || c == '[') { paren_depth += 1; }
        else if(c == ')' || c == ']') { if(paren_depth > 0) paren_depth -= 1; }
        else if(c == ',' && paren_depth == 0) { comma_pos = j; }
      }
      
      if(comma_pos < expr_text.size)
      {
        String8 maybe_spec = str8_skip_chop_whitespace(str8(expr_text.str + comma_pos + 1, expr_text.size - comma_pos - 1));
        // format specs are short alphanumeric tokens or [N]
        B32 is_format_spec = (maybe_spec.size > 0 && maybe_spec.size <= 10);
        if(is_format_spec)
        {
          for(U64 k = 0; k < maybe_spec.size; k += 1)
          {
            U8 sc = maybe_spec.str[k];
            if(!((sc >= 'a' && sc <= 'z') || (sc >= 'A' && sc <= 'Z') ||
                 (sc >= '0' && sc <= '9') || sc == '[' || sc == ']' || sc == '!'))
            {
              is_format_spec = 0;
              break;
            }
          }
        }
        if(is_format_spec)
        {
          part->text = str8_skip_chop_whitespace(str8(expr_text.str, comma_pos));
          part->format_spec = maybe_spec;
        }
        else
        {
          part->text = expr_text;
        }
      }
      else
      {
        part->text = expr_text;
      }
      
      SLLQueuePush(ds->first_part, ds->last_part, part);
    }
    else
    {
      // literal text run
      U64 start = i;
      while(i < raw.size && raw.str[i] != '{' && raw.str[i] != '}')
      {
        i += 1;
      }
      // handle lone } at end
      if(i < raw.size && raw.str[i] == '}' && (i + 1 >= raw.size || raw.str[i+1] != '}'))
      {
        i += 1;
      }
      
      NV_DisplayPart *part = push_array(arena, NV_DisplayPart, 1);
      part->kind = NV_DisplayPartKind_Literal;
      part->text = str8(raw.str + start, i - start);
      SLLQueuePush(ds->first_part, ds->last_part, part);
    }
  }
  
  return ds;
}

////////////////////////////////
//~ NatVis CustomListItems Statement Parsing

internal NV_CLStatement *
nv_cl_statements_from_xml(Arena *arena, NV_XMLNode *parent)
{
  NV_CLStatement *first = 0;
  NV_CLStatement *last = 0;
  
  for(NV_XMLNode *child = parent->first; child != 0; child = child->next)
  {
    NV_CLStatement *stmt = push_array(arena, NV_CLStatement, 1);
    
    if(str8_match(child->tag, str8_lit("Loop"), 0))
    {
      stmt->kind = NV_CLStatementKind_Loop;
      stmt->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
      stmt->first_child = nv_cl_statements_from_xml(arena, child);
    }
    else if(str8_match(child->tag, str8_lit("If"), 0))
    {
      stmt->kind = NV_CLStatementKind_If;
      stmt->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
      stmt->first_child = nv_cl_statements_from_xml(arena, child);
    }
    else if(str8_match(child->tag, str8_lit("Elseif"), 0))
    {
      stmt->kind = NV_CLStatementKind_Elseif;
      stmt->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
      stmt->first_child = nv_cl_statements_from_xml(arena, child);
    }
    else if(str8_match(child->tag, str8_lit("Else"), 0))
    {
      stmt->kind = NV_CLStatementKind_Else;
      stmt->first_child = nv_cl_statements_from_xml(arena, child);
    }
    else if(str8_match(child->tag, str8_lit("Exec"), 0))
    {
      stmt->kind = NV_CLStatementKind_Exec;
      stmt->expression = child->text;
    }
    else if(str8_match(child->tag, str8_lit("Break"), 0))
    {
      stmt->kind = NV_CLStatementKind_Break;
      stmt->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
    }
    else if(str8_match(child->tag, str8_lit("Item"), 0))
    {
      stmt->kind = NV_CLStatementKind_Item;
      stmt->item_name = nv_xml_attr_from_key(child, str8_lit("Name"));
      stmt->expression = child->text;
      stmt->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
    }
    else
    {
      // skip Variable, Size, Skip (handled separately or not yet supported)
      continue;
    }
    
    SLLQueuePush(first, last, stmt);
  }
  
  return first;
}

////////////////////////////////
//~ NatVis Expand Parsing

internal NV_Expand *
nv_expand_from_xml(Arena *arena, NV_XMLNode *expand_node)
{
  if(expand_node == 0) { return 0; }
  
  NV_Expand *expand = push_array(arena, NV_Expand, 1);
  String8 hrv = nv_xml_attr_from_key(expand_node, str8_lit("HideRawView"));
  expand->hide_raw_view = (hrv.size > 0 && (hrv.str[0] == 't' || hrv.str[0] == 'T' || hrv.str[0] == '1'));
  
  for(NV_XMLNode *child = expand_node->first; child != 0; child = child->next)
  {
    NV_ExpandItem *item = push_array(arena, NV_ExpandItem, 1);
    item->condition = nv_xml_attr_from_key(child, str8_lit("Condition"));
    item->is_optional = nv_xml_attr_exists(child, str8_lit("Optional"));
    
    if(str8_match(child->tag, str8_lit("Item"), 0))
    {
      item->kind = NV_ExpandItemKind_Item;
      item->item.name = nv_xml_attr_from_key(child, str8_lit("Name"));
      item->item.expression = child->text;
    }
    else if(str8_match(child->tag, str8_lit("ArrayItems"), 0))
    {
      item->kind = NV_ExpandItemKind_ArrayItems;
      NV_XMLNode *size_n = nv_xml_child_from_tag(child, str8_lit("Size"));
      NV_XMLNode *vp_n = nv_xml_child_from_tag(child, str8_lit("ValuePointer"));
      NV_XMLNode *lb_n = nv_xml_child_from_tag(child, str8_lit("LowerBound"));
      item->array_items.size_expr = size_n ? size_n->text : str8_zero();
      item->array_items.value_pointer_expr = vp_n ? vp_n->text : str8_zero();
      item->array_items.lower_bound_expr = lb_n ? lb_n->text : str8_zero();
    }
    else if(str8_match(child->tag, str8_lit("IndexListItems"), 0))
    {
      item->kind = NV_ExpandItemKind_IndexListItems;
      NV_XMLNode *size_n = nv_xml_child_from_tag(child, str8_lit("Size"));
      NV_XMLNode *vn_n = nv_xml_child_from_tag(child, str8_lit("ValueNode"));
      item->index_list_items.size_expr = size_n ? size_n->text : str8_zero();
      item->index_list_items.value_node_expr = vn_n ? vn_n->text : str8_zero();
    }
    else if(str8_match(child->tag, str8_lit("LinkedListItems"), 0))
    {
      item->kind = NV_ExpandItemKind_LinkedListItems;
      NV_XMLNode *size_n = nv_xml_child_from_tag(child, str8_lit("Size"));
      NV_XMLNode *head_n = nv_xml_child_from_tag(child, str8_lit("HeadPointer"));
      NV_XMLNode *next_n = nv_xml_child_from_tag(child, str8_lit("NextPointer"));
      NV_XMLNode *val_n = nv_xml_child_from_tag(child, str8_lit("ValueNode"));
      item->linked_list_items.size_expr = size_n ? size_n->text : str8_zero();
      item->linked_list_items.head_pointer_expr = head_n ? head_n->text : str8_zero();
      item->linked_list_items.next_pointer_expr = next_n ? next_n->text : str8_zero();
      item->linked_list_items.value_node_expr = val_n ? val_n->text : str8_zero();
    }
    else if(str8_match(child->tag, str8_lit("TreeItems"), 0))
    {
      item->kind = NV_ExpandItemKind_TreeItems;
      NV_XMLNode *size_n = nv_xml_child_from_tag(child, str8_lit("Size"));
      NV_XMLNode *head_n = nv_xml_child_from_tag(child, str8_lit("HeadPointer"));
      NV_XMLNode *left_n = nv_xml_child_from_tag(child, str8_lit("LeftPointer"));
      NV_XMLNode *right_n = nv_xml_child_from_tag(child, str8_lit("RightPointer"));
      NV_XMLNode *val_n = nv_xml_child_from_tag(child, str8_lit("ValueNode"));
      item->tree_items.size_expr = size_n ? size_n->text : str8_zero();
      item->tree_items.head_pointer_expr = head_n ? head_n->text : str8_zero();
      item->tree_items.left_pointer_expr = left_n ? left_n->text : str8_zero();
      item->tree_items.right_pointer_expr = right_n ? right_n->text : str8_zero();
      item->tree_items.value_node_expr = val_n ? val_n->text : str8_zero();
    }
    else if(str8_match(child->tag, str8_lit("CustomListItems"), 0))
    {
      item->kind = NV_ExpandItemKind_CustomListItems;
      String8 mipv = nv_xml_attr_from_key(child, str8_lit("MaxItemsPerView"));
      item->custom_list_items.max_items_per_view = 5000;
      if(mipv.size > 0)
      {
        U64 val = 0;
        for(U64 k = 0; k < mipv.size; k += 1)
        {
          if(mipv.str[k] >= '0' && mipv.str[k] <= '9')
          {
            val = val * 10 + (mipv.str[k] - '0');
          }
        }
        if(val > 0) { item->custom_list_items.max_items_per_view = val; }
      }
      
      // parse Variables
      for(NV_XMLNode *vc = child->first; vc != 0; vc = vc->next)
      {
        if(str8_match(vc->tag, str8_lit("Variable"), 0))
        {
          NV_CLVariable *var = push_array(arena, NV_CLVariable, 1);
          var->name = nv_xml_attr_from_key(vc, str8_lit("Name"));
          var->initial_value = nv_xml_attr_from_key(vc, str8_lit("InitialValue"));
          SLLQueuePush(item->custom_list_items.first_variable, item->custom_list_items.last_variable, var);
        }
      }
      
      // parse Size
      NV_XMLNode *size_n = nv_xml_child_from_tag(child, str8_lit("Size"));
      item->custom_list_items.size_expr = size_n ? size_n->text : str8_zero();
      
      // parse statements (Loop, If, Exec, Break, Item, ...)
      item->custom_list_items.first_statement = nv_cl_statements_from_xml(arena, child);
    }
    else if(str8_match(child->tag, str8_lit("Synthetic"), 0))
    {
      item->kind = NV_ExpandItemKind_Synthetic;
      item->synthetic.name = nv_xml_attr_from_key(child, str8_lit("Name"));
      item->synthetic.watch_expression = nv_xml_attr_from_key(child, str8_lit("Expression"));
      
      for(NV_XMLNode *sc = child->first; sc != 0; sc = sc->next)
      {
        if(str8_match(sc->tag, str8_lit("DisplayString"), 0))
        {
          NV_DisplayString *ds = nv_display_string_from_xml(arena, sc);
          SLLQueuePush(item->synthetic.first_display_string, item->synthetic.last_display_string, ds);
        }
        else if(str8_match(sc->tag, str8_lit("Expand"), 0))
        {
          item->synthetic.expand = nv_expand_from_xml(arena, sc);
        }
      }
    }
    else if(str8_match(child->tag, str8_lit("ExpandedItem"), 0))
    {
      item->kind = NV_ExpandItemKind_ExpandedItem;
      item->expanded_item.expression = child->text;
    }
    else
    {
      continue;
    }
    
    SLLQueuePush(expand->first_item, expand->last_item, item);
    expand->item_count += 1;
  }
  
  return expand;
}

////////////////////////////////
//~ NatVis Priority Parsing

internal NV_Priority
nv_priority_from_string(String8 s)
{
  if(str8_match(s, str8_lit("Low"), StringMatchFlag_CaseInsensitive))        { return NV_Priority_Low; }
  if(str8_match(s, str8_lit("MediumLow"), StringMatchFlag_CaseInsensitive))  { return NV_Priority_MediumLow; }
  if(str8_match(s, str8_lit("Medium"), StringMatchFlag_CaseInsensitive))     { return NV_Priority_Medium; }
  if(str8_match(s, str8_lit("MediumHigh"), StringMatchFlag_CaseInsensitive)) { return NV_Priority_MediumHigh; }
  if(str8_match(s, str8_lit("High"), StringMatchFlag_CaseInsensitive))       { return NV_Priority_High; }
  return NV_Priority_Medium;
}

////////////////////////////////
//~ NatVis File from XML

internal NV_File *
nv_file_from_xml(Arena *arena, NV_XMLNode *xml_root, String8 file_path)
{
  NV_File *file = push_array(arena, NV_File, 1);
  file->arena = arena;
  file->path = str8_copy(arena, file_path);
  
  // find AutoVisualizer
  NV_XMLNode *av = nv_xml_child_from_tag(xml_root, str8_lit("AutoVisualizer"));
  if(av == 0) { av = xml_root; }
  
  for(NV_XMLNode *type_node = av->first; type_node != 0; type_node = type_node->next)
  {
    if(!str8_match(type_node->tag, str8_lit("Type"), 0)) { continue; }
    
    NV_TypeDef *td = push_array(arena, NV_TypeDef, 1);
    td->name = nv_xml_attr_from_key(type_node, str8_lit("Name"));
    td->priority = nv_priority_from_string(nv_xml_attr_from_key(type_node, str8_lit("Priority")));
    
    String8 inh = nv_xml_attr_from_key(type_node, str8_lit("Inheritable"));
    td->inheritable = (inh.size == 0 || inh.str[0] == 't' || inh.str[0] == 'T' || inh.str[0] == '1');
    
    // AlternativeType + Intrinsic detection
    for(NV_XMLNode *ac = type_node->first; ac != 0; ac = ac->next)
    {
      if(str8_match(ac->tag, str8_lit("AlternativeType"), 0))
      {
        String8 alt_name = nv_xml_attr_from_key(ac, str8_lit("Name"));
        if(alt_name.size > 0)
        {
          str8_list_push(arena, &td->alternative_names, alt_name);
        }
      }
      else if(str8_match(ac->tag, str8_lit("Intrinsic"), 0))
      {
        td->has_intrinsic = 1;
      }
    }
    
    // DisplayString
    for(NV_XMLNode *dc = type_node->first; dc != 0; dc = dc->next)
    {
      if(str8_match(dc->tag, str8_lit("DisplayString"), 0))
      {
        NV_DisplayString *ds = nv_display_string_from_xml(arena, dc);
        SLLQueuePush(td->first_display_string, td->last_display_string, ds);
      }
    }
    
    // StringView
    NV_XMLNode *sv_node = nv_xml_child_from_tag(type_node, str8_lit("StringView"));
    if(sv_node != 0)
    {
      NV_StringView *sv = push_array(arena, NV_StringView, 1);
      sv->expression = sv_node->text;
      sv->condition = nv_xml_attr_from_key(sv_node, str8_lit("Condition"));
      td->string_view = sv;
    }
    
    // Expand
    NV_XMLNode *exp_node = nv_xml_child_from_tag(type_node, str8_lit("Expand"));
    td->expand = nv_expand_from_xml(arena, exp_node);
    
    SLLQueuePush(file->first_type, file->last_type, td);
    file->type_count += 1;
  }
  
  return file;
}

////////////////////////////////
//~ NatVis Type Matching

internal NV_TypeMatch
nv_type_match(String8 pattern, String8 type_name)
{
  NV_TypeMatch result = {0};
  
  // wildcard matching: * in pattern matches template arguments
  // $T1, $T2 etc. are derived from matched wildcards left-to-right
  U64 pi = 0;
  U64 ti = 0;
  
  while(pi < pattern.size && ti < type_name.size)
  {
    if(pattern.str[pi] == '*')
    {
      pi += 1;
      
      // capture wildcard: match until the next literal segment in pattern
      // or end of pattern
      U64 capture_start = ti;
      
      if(pi >= pattern.size)
      {
        // wildcard at end: consume rest of type_name
        if(result.template_arg_count < ArrayCount(result.template_args))
        {
          result.template_args[result.template_arg_count] = str8(type_name.str + capture_start, type_name.size - capture_start);
          result.template_arg_count += 1;
        }
        ti = type_name.size;
      }
      else
      {
        // find next literal char in pattern
        U8 next_literal = pattern.str[pi];
        
        // match respecting angle bracket/paren nesting
        S64 depth = 0;
        B32 found = 0;
        
        while(ti < type_name.size)
        {
          U8 c = type_name.str[ti];
          
          // check for match before adjusting depth for closers,
          // so '>' at depth 0 is found correctly
          if(depth == 0 && c == next_literal)
          {
            found = 1;
            break;
          }
          
          if(c == '<' || c == '(') { depth += 1; }
          else if(c == '>' || c == ')') { depth -= 1; }
          
          ti += 1;
        }
        
        if(!found) { return result; }
        
        if(result.template_arg_count < ArrayCount(result.template_args))
        {
          result.template_args[result.template_arg_count] = str8(type_name.str + capture_start, ti - capture_start);
          result.template_arg_count += 1;
        }
      }
    }
    else
    {
      // literal compare (whitespace-insensitive around commas and angle brackets)
      if(pattern.str[pi] != type_name.str[ti])
      {
        // try skipping whitespace in type_name
        if(type_name.str[ti] == ' ' && ti + 1 < type_name.size)
        {
          ti += 1;
          continue;
        }
        if(pattern.str[pi] == ' ' && pi + 1 < pattern.size)
        {
          pi += 1;
          continue;
        }
        return result;
      }
      pi += 1;
      ti += 1;
    }
  }
  
  // skip trailing whitespace
  while(pi < pattern.size && pattern.str[pi] == ' ') { pi += 1; }
  while(ti < type_name.size && type_name.str[ti] == ' ') { ti += 1; }
  
  result.matched = (pi == pattern.size && ti == type_name.size);
  return result;
}

////////////////////////////////
//~ NatVis TypeDef Lookup

internal NV_TypeDef *
nv_type_def_from_type_name(NV_File *file, String8 type_name)
{
  if(file == 0) { return 0; }
  
  NV_TypeDef *best = 0;
  NV_Priority best_priority = NV_Priority_Low;
  
  for(NV_TypeDef *td = file->first_type; td != 0; td = td->next)
  {
    // check main name
    NV_TypeMatch m = nv_type_match(td->name, type_name);
    if(m.matched && td->priority >= best_priority)
    {
      best = td;
      best_priority = td->priority;
    }
    
    // check alternative names
    for(String8Node *alt = td->alternative_names.first; alt != 0; alt = alt->next)
    {
      NV_TypeMatch am = nv_type_match(alt->string, type_name);
      if(am.matched && td->priority >= best_priority)
      {
        best = td;
        best_priority = td->priority;
      }
    }
  }
  
  return best;
}
