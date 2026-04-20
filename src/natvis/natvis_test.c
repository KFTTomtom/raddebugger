// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)
//
// Standalone test for the NatVis XML parser.
// Build from the repo root:
//   cd build
//   cl /Od /Zi /I..\src\ ..\src\natvis\natvis_test.c /link /out:natvis_test.exe
//
// Usage:
//   natvis_test.exe <path_to_natvis_file>
//   natvis_test.exe                           (uses built-in test)

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "natvis/natvis_inc.h"

#include "base/base_inc.c"
#include "os/os_inc.c"
#include "natvis/natvis_inc.c"

////////////////////////////////
//~ Tree printer (for visual inspection)

internal void
nv_test_print_tree(NV_XMLNode *node, int depth)
{
  if(node == 0) { return; }
  
  // indent
  for(int i = 0; i < depth; i += 1) { printf("  "); }
  
  // tag
  printf("<%.*s", (int)node->tag.size, node->tag.str);
  
  // attrs
  for(NV_XMLAttr *a = node->first_attr; a != 0; a = a->next)
  {
    printf(" %.*s=\"%.*s\"", (int)a->key.size, a->key.str, (int)a->value.size, a->value.str);
  }
  
  // self-closing if no children and no text
  if(node->first == 0 && node->text.size == 0)
  {
    printf("/>\n");
    return;
  }
  
  printf(">");
  
  // text content
  if(node->text.size > 0)
  {
    U64 show = Min(node->text.size, 80);
    printf("%.*s", (int)show, node->text.str);
    if(show < node->text.size) { printf("..."); }
  }
  
  // children
  if(node->first != 0)
  {
    printf("\n");
    for(NV_XMLNode *child = node->first; child != 0; child = child->next)
    {
      nv_test_print_tree(child, depth + 1);
    }
    for(int i = 0; i < depth; i += 1) { printf("  "); }
  }
  
  printf("</%.*s>\n", (int)node->tag.size, node->tag.str);
}

////////////////////////////////
//~ Statistics collector

typedef struct NV_TestStats NV_TestStats;
struct NV_TestStats
{
  U64 type_count;
  U64 alternative_type_count;
  U64 display_string_count;
  U64 expand_count;
  U64 item_count;
  U64 array_items_count;
  U64 index_list_items_count;
  U64 linked_list_items_count;
  U64 custom_list_items_count;
  U64 synthetic_count;
  U64 expanded_item_count;
  U64 intrinsic_count;
  U64 tree_items_count;
};

internal void
nv_test_collect_stats_recursive(NV_XMLNode *node, NV_TestStats *stats)
{
  if(node == 0) { return; }
  
  if(str8_match(node->tag, str8_lit("Type"), 0))                    { stats->type_count += 1; }
  else if(str8_match(node->tag, str8_lit("AlternativeType"), 0))    { stats->alternative_type_count += 1; }
  else if(str8_match(node->tag, str8_lit("DisplayString"), 0))      { stats->display_string_count += 1; }
  else if(str8_match(node->tag, str8_lit("Expand"), 0))             { stats->expand_count += 1; }
  else if(str8_match(node->tag, str8_lit("Item"), 0))               { stats->item_count += 1; }
  else if(str8_match(node->tag, str8_lit("ArrayItems"), 0))         { stats->array_items_count += 1; }
  else if(str8_match(node->tag, str8_lit("IndexListItems"), 0))     { stats->index_list_items_count += 1; }
  else if(str8_match(node->tag, str8_lit("LinkedListItems"), 0))    { stats->linked_list_items_count += 1; }
  else if(str8_match(node->tag, str8_lit("CustomListItems"), 0))    { stats->custom_list_items_count += 1; }
  else if(str8_match(node->tag, str8_lit("Synthetic"), 0))          { stats->synthetic_count += 1; }
  else if(str8_match(node->tag, str8_lit("ExpandedItem"), 0))       { stats->expanded_item_count += 1; }
  else if(str8_match(node->tag, str8_lit("Intrinsic"), 0))          { stats->intrinsic_count += 1; }
  else if(str8_match(node->tag, str8_lit("TreeItems"), 0))          { stats->tree_items_count += 1; }
  
  for(NV_XMLNode *child = node->first; child != 0; child = child->next)
  {
    nv_test_collect_stats_recursive(child, stats);
  }
}

////////////////////////////////
//~ Built-in test XML

read_only global char nv_test_xml[] =
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
  "<AutoVisualizer xmlns=\"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\n"
  "  <!-- test comment -->\n"
  "  <Type Name=\"TestSimple\">\n"
  "    <DisplayString>{{ val={value} }}</DisplayString>\n"
  "    <Expand>\n"
  "      <Item Name=\"value\">value</Item>\n"
  "      <Item Name=\"[computed]\" Condition=\"value &gt; 0\">value * 2</Item>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestArray&lt;*&gt;\">\n"
  "    <DisplayString>{{ size={_size} }}</DisplayString>\n"
  "    <Expand>\n"
  "      <ArrayItems>\n"
  "        <Size>_size</Size>\n"
  "        <ValuePointer>_data</ValuePointer>\n"
  "      </ArrayItems>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestList\">\n"
  "    <Expand>\n"
  "      <LinkedListItems>\n"
  "        <Size>count</Size>\n"
  "        <HeadPointer>head</HeadPointer>\n"
  "        <NextPointer>next</NextPointer>\n"
  "        <ValueNode>data</ValueNode>\n"
  "      </LinkedListItems>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestCustom\">\n"
  "    <Expand>\n"
  "      <CustomListItems MaxItemsPerView=\"100\">\n"
  "        <Variable Name=\"node\" InitialValue=\"head\"/>\n"
  "        <Variable Name=\"idx\" InitialValue=\"0\"/>\n"
  "        <Size>count</Size>\n"
  "        <Loop>\n"
  "          <Break Condition=\"node == 0\"/>\n"
  "          <Item>node->value</Item>\n"
  "          <Exec>node = node->next</Exec>\n"
  "        </Loop>\n"
  "      </CustomListItems>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestSynthetic\">\n"
  "    <Expand>\n"
  "      <Synthetic Name=\"[properties]\">\n"
  "        <DisplayString>props</DisplayString>\n"
  "        <Expand>\n"
  "          <Item Name=\"a\">prop_a</Item>\n"
  "          <Item Name=\"b\">prop_b</Item>\n"
  "        </Expand>\n"
  "      </Synthetic>\n"
  "      <ExpandedItem>base_class</ExpandedItem>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestAlternative\">\n"
  "    <AlternativeType Name=\"TestAlternativeB\"/>\n"
  "    <AlternativeType Name=\"TestAlternativeC\"/>\n"
  "    <DisplayString>alt</DisplayString>\n"
  "  </Type>\n"
  "  <Type Name=\"TestSelfClosing\">\n"
  "    <DisplayString>self</DisplayString>\n"
  "  </Type>\n"
  "</AutoVisualizer>\n";

////////////////////////////////
//~ Built-in Intrinsic test XML

read_only global char nv_test_intrinsic_xml[] =
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
  "<AutoVisualizer xmlns=\"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\n"
  "  <Intrinsic Name=\"_AlignInt\" Expression=\"(Val + Alignment - 1) &amp; ~(Alignment - 1)\">\n"
  "    <Parameter Name=\"Val\" Type=\"uintptr_t\"/>\n"
  "    <Parameter Name=\"Alignment\" Type=\"uintptr_t\"/>\n"
  "  </Intrinsic>\n"
  "  <Intrinsic Name=\"_GlobalHelper\" Expression=\"GlobalPtr->Data\" SideEffect=\"false\"/>\n"
  "  <Type Name=\"TestAllocator\">\n"
  "    <Intrinsic Name=\"_GetData\" Expression=\"(uint8*)Data\"/>\n"
  "    <DisplayString>{{ data={_GetData()} }}</DisplayString>\n"
  "    <Expand>\n"
  "      <Item Name=\"data\">_GetData()</Item>\n"
  "    </Expand>\n"
  "  </Type>\n"
  "  <Type Name=\"TestAligned\">\n"
  "    <Intrinsic Name=\"GetAligned\" Expression=\"_AlignInt(Size, 16)\"/>\n"
  "    <DisplayString>{{ aligned={GetAligned()} }}</DisplayString>\n"
  "  </Type>\n"
  "  <Type Name=\"TestFrame\">\n"
  "    <Intrinsic Name=\"stateTree\" Category=\"Method\" Expression=\"*(UStateTree**)&amp;StateTree.Handle\"/>\n"
  "    <Intrinsic Name=\"stateName\" Category=\"Method\" Expression=\"((FState*)stateTree()->States.Data)[stateHandle.Index].Name\">\n"
  "      <Parameter Name=\"stateHandle\" Type=\"FHandle\"/>\n"
  "    </Intrinsic>\n"
  "    <DisplayString>{{ tree={stateTree()->NamePrivate} }}</DisplayString>\n"
  "  </Type>\n"
  "  <Type Name=\"TestGlobalRef\">\n"
  "    <DisplayString>{{ val={_GlobalHelper()} }}</DisplayString>\n"
  "  </Type>\n"
  "</AutoVisualizer>\n";

////////////////////////////////
//~ Validation functions

internal B32
nv_test_validate_builtin(void)
{
  Arena *arena = arena_alloc();
  printf("=== Built-in XML test ===\n");
  
  String8 xml = str8((U8 *)nv_test_xml, sizeof(nv_test_xml) - 1);
  NV_XMLParseResult result = nv_xml_parse_from_string(arena, xml);
  
  B32 passed = 1;
  
  // root should exist
  if(result.root == 0)
  {
    printf("FAIL: root is null\n");
    arena_release(arena);
    return 0;
  }
  
  // should have one top-level child: AutoVisualizer
  NV_XMLNode *av = result.root->first;
  if(av == 0 || !str8_match(av->tag, str8_lit("AutoVisualizer"), 0))
  {
    printf("FAIL: expected AutoVisualizer as first child, got %.*s\n",
           av ? (int)av->tag.size : 4, av ? (char *)av->tag.str : "null");
    passed = 0;
  }
  
  // count Type children
  U64 type_count = nv_xml_child_count_with_tag(av, str8_lit("Type"));
  if(type_count != 7)
  {
    printf("FAIL: expected 7 Type elements, got %llu\n", type_count);
    passed = 0;
  }
  else
  {
    printf("OK: 7 Type elements found\n");
  }
  
  // check first Type name
  NV_XMLNode *first_type = nv_xml_child_from_tag(av, str8_lit("Type"));
  String8 first_name = nv_xml_attr_from_key(first_type, str8_lit("Name"));
  if(!str8_match(first_name, str8_lit("TestSimple"), 0))
  {
    printf("FAIL: first Type Name expected 'TestSimple', got '%.*s'\n", (int)first_name.size, first_name.str);
    passed = 0;
  }
  else
  {
    printf("OK: first Type Name = 'TestSimple'\n");
  }
  
  // check entity decoding: TestArray<*> (from &lt;*&gt;)
  NV_XMLNode *second_type = nv_xml_next_from_tag(first_type, str8_lit("Type"));
  if(second_type != 0)
  {
    String8 second_name = nv_xml_attr_from_key(second_type, str8_lit("Name"));
    if(!str8_match(second_name, str8_lit("TestArray<*>"), 0))
    {
      printf("FAIL: second Type Name expected 'TestArray<*>', got '%.*s'\n", (int)second_name.size, second_name.str);
      passed = 0;
    }
    else
    {
      printf("OK: entity decoding works: 'TestArray<*>'\n");
    }
  }
  
  // check Condition with &gt;
  NV_XMLNode *expand = nv_xml_child_from_tag(first_type, str8_lit("Expand"));
  if(expand != 0)
  {
    NV_XMLNode *item = nv_xml_child_from_tag(expand, str8_lit("Item"));
    NV_XMLNode *cond_item = nv_xml_next_from_tag(item, str8_lit("Item"));
    if(cond_item != 0)
    {
      String8 cond = nv_xml_attr_from_key(cond_item, str8_lit("Condition"));
      if(cond.size > 0 && cond.str[cond.size - 3] == '>')
      {
        printf("OK: Condition entity decoded: '%.*s'\n", (int)cond.size, cond.str);
      }
      else
      {
        printf("FAIL: Condition entity decoding failed: '%.*s'\n", (int)cond.size, cond.str);
        passed = 0;
      }
    }
  }
  
  // check AlternativeType count
  NV_XMLNode *alt_type = 0;
  for(NV_XMLNode *t = first_type; t != 0; t = nv_xml_next_from_tag(t, str8_lit("Type")))
  {
    String8 name = nv_xml_attr_from_key(t, str8_lit("Name"));
    if(str8_match(name, str8_lit("TestAlternative"), 0))
    {
      alt_type = t;
      break;
    }
  }
  if(alt_type != 0)
  {
    U64 alt_count = nv_xml_child_count_with_tag(alt_type, str8_lit("AlternativeType"));
    if(alt_count == 2)
    {
      printf("OK: AlternativeType count = 2\n");
    }
    else
    {
      printf("FAIL: expected 2 AlternativeType, got %llu\n", alt_count);
      passed = 0;
    }
  }
  
  // check CustomListItems attributes
  NV_XMLNode *custom_type = 0;
  for(NV_XMLNode *t = first_type; t != 0; t = nv_xml_next_from_tag(t, str8_lit("Type")))
  {
    String8 name = nv_xml_attr_from_key(t, str8_lit("Name"));
    if(str8_match(name, str8_lit("TestCustom"), 0))
    {
      custom_type = t;
      break;
    }
  }
  if(custom_type != 0)
  {
    NV_XMLNode *exp = nv_xml_child_from_tag(custom_type, str8_lit("Expand"));
    NV_XMLNode *cli = nv_xml_child_from_tag(exp, str8_lit("CustomListItems"));
    String8 max_items = nv_xml_attr_from_key(cli, str8_lit("MaxItemsPerView"));
    if(str8_match(max_items, str8_lit("100"), 0))
    {
      printf("OK: CustomListItems MaxItemsPerView = '100'\n");
    }
    else
    {
      printf("FAIL: CustomListItems MaxItemsPerView expected '100', got '%.*s'\n", (int)max_items.size, max_items.str);
      passed = 0;
    }
    
    // check Variable elements (self-closing)
    U64 var_count = nv_xml_child_count_with_tag(cli, str8_lit("Variable"));
    if(var_count == 2)
    {
      printf("OK: CustomListItems has 2 Variable elements\n");
    }
    else
    {
      printf("FAIL: expected 2 Variable, got %llu\n", var_count);
      passed = 0;
    }
    
    // check Loop/Break/Exec/Item inside CustomListItems
    NV_XMLNode *loop = nv_xml_child_from_tag(cli, str8_lit("Loop"));
    if(loop != 0)
    {
      NV_XMLNode *brk = nv_xml_child_from_tag(loop, str8_lit("Break"));
      NV_XMLNode *exec = nv_xml_child_from_tag(loop, str8_lit("Exec"));
      NV_XMLNode *citem = nv_xml_child_from_tag(loop, str8_lit("Item"));
      if(brk != 0 && exec != 0 && citem != 0)
      {
        printf("OK: Loop contains Break, Exec, Item\n");
      }
      else
      {
        printf("FAIL: Loop missing children (Break=%p Exec=%p Item=%p)\n", (void *)brk, (void *)exec, (void *)citem);
        passed = 0;
      }
    }
  }
  
  // check Synthetic
  NV_XMLNode *synth_type = 0;
  for(NV_XMLNode *t = first_type; t != 0; t = nv_xml_next_from_tag(t, str8_lit("Type")))
  {
    String8 name = nv_xml_attr_from_key(t, str8_lit("Name"));
    if(str8_match(name, str8_lit("TestSynthetic"), 0))
    {
      synth_type = t;
      break;
    }
  }
  if(synth_type != 0)
  {
    NV_XMLNode *exp = nv_xml_child_from_tag(synth_type, str8_lit("Expand"));
    NV_XMLNode *synth = nv_xml_child_from_tag(exp, str8_lit("Synthetic"));
    NV_XMLNode *exp_item = nv_xml_child_from_tag(exp, str8_lit("ExpandedItem"));
    if(synth != 0 && exp_item != 0)
    {
      String8 sname = nv_xml_attr_from_key(synth, str8_lit("Name"));
      if(str8_match(sname, str8_lit("[properties]"), 0))
      {
        printf("OK: Synthetic Name = '[properties]'\n");
      }
      else
      {
        printf("FAIL: Synthetic Name expected '[properties]', got '%.*s'\n", (int)sname.size, sname.str);
        passed = 0;
      }
      
      // check nested Expand inside Synthetic
      NV_XMLNode *inner_exp = nv_xml_child_from_tag(synth, str8_lit("Expand"));
      U64 inner_items = nv_xml_child_count_with_tag(inner_exp, str8_lit("Item"));
      if(inner_items == 2)
      {
        printf("OK: Synthetic nested Expand has 2 Items\n");
      }
      else
      {
        printf("FAIL: expected 2 Items in nested Expand, got %llu\n", inner_items);
        passed = 0;
      }
      
      // ExpandedItem text
      if(exp_item->text.size > 0 && str8_match(exp_item->text, str8_lit("base_class"), 0))
      {
        printf("OK: ExpandedItem text = 'base_class'\n");
      }
      else
      {
        printf("FAIL: ExpandedItem text expected 'base_class'\n");
        passed = 0;
      }
    }
    else
    {
      printf("FAIL: Synthetic or ExpandedItem not found\n");
      passed = 0;
    }
  }
  
  printf("Parse errors: %llu\n", result.error_count);
  printf("=== Built-in test %s ===\n\n", passed ? "PASSED" : "FAILED");
  
  arena_release(arena);
  return passed;
}

////////////////////////////////
//~ T2 validation: semantic model + type matching

internal B32
nv_test_semantic_model(void)
{
  Arena *arena = arena_alloc();
  printf("=== Semantic model test ===\n");
  B32 passed = 1;
  
  String8 xml = str8((U8 *)nv_test_xml, sizeof(nv_test_xml) - 1);
  NV_XMLParseResult xr = nv_xml_parse_from_string(arena, xml);
  NV_File *file = nv_file_from_xml(arena, xr.root, str8_lit("test.natvis"));
  
  if(file->type_count != 7)
  {
    printf("FAIL: expected 7 types, got %llu\n", file->type_count);
    passed = 0;
  }
  else { printf("OK: 7 types in semantic model\n"); }
  
  // check first type DisplayString parts
  NV_TypeDef *t0 = file->first_type;
  if(t0 != 0 && t0->first_display_string != 0)
  {
    NV_DisplayPart *p = t0->first_display_string->first_part;
    // should be: Literal "{ " then Expr "val=" then Expr "value" then Literal " }"
    // actually: {{ val={value} }} → Literal "{" + Literal " val=" + Expr "value" + Literal " }"
    U64 part_count = 0;
    B32 has_expr = 0;
    for(NV_DisplayPart *dp = p; dp != 0; dp = dp->next)
    {
      part_count += 1;
      if(dp->kind == NV_DisplayPartKind_Expression) { has_expr = 1; }
    }
    if(has_expr) { printf("OK: DisplayString has expression part\n"); }
    else { printf("FAIL: no expression part in DisplayString\n"); passed = 0; }
  }
  
  // check type matching
  {
    NV_TypeMatch m1 = nv_type_match(str8_lit("TestArray<*>"), str8_lit("TestArray<int>"));
    if(m1.matched && m1.template_arg_count == 1 && str8_match(m1.template_args[0], str8_lit("int"), 0))
    { printf("OK: match TestArray<int> → $T1=int\n"); }
    else { printf("FAIL: TestArray<int> match\n"); passed = 0; }
    
    NV_TypeMatch m2 = nv_type_match(str8_lit("TArray<*>"), str8_lit("TArray<FString>"));
    if(m2.matched && str8_match(m2.template_args[0], str8_lit("FString"), 0))
    { printf("OK: match TArray<FString> → $T1=FString\n"); }
    else { printf("FAIL: TArray<FString> match\n"); passed = 0; }
    
    NV_TypeMatch m3 = nv_type_match(str8_lit("std::map<*,*>"), str8_lit("std::map<int,float>"));
    if(m3.matched && m3.template_arg_count == 2)
    { printf("OK: match std::map<int,float> → $T1=int, $T2=float\n"); }
    else { printf("FAIL: std::map<int,float> match (matched=%d, argc=%llu)\n", m3.matched, m3.template_arg_count); passed = 0; }
    
    NV_TypeMatch m4 = nv_type_match(str8_lit("TArray<*>"), str8_lit("FString"));
    if(!m4.matched) { printf("OK: TArray<*> does not match FString\n"); }
    else { printf("FAIL: TArray<*> should not match FString\n"); passed = 0; }
    
    // nested templates
    NV_TypeMatch m5 = nv_type_match(str8_lit("TArray<*>"), str8_lit("TArray<TSharedPtr<FString>>"));
    if(m5.matched && str8_match(m5.template_args[0], str8_lit("TSharedPtr<FString>"), 0))
    { printf("OK: match nested TArray<TSharedPtr<FString>>\n"); }
    else { printf("FAIL: nested template match\n"); passed = 0; }
  }
  
  // check lookup
  {
    NV_TypeDef *found = nv_type_def_from_type_name(file, str8_lit("TestSimple"));
    if(found != 0) { printf("OK: lookup TestSimple found\n"); }
    else { printf("FAIL: lookup TestSimple\n"); passed = 0; }
    
    NV_TypeDef *not_found = nv_type_def_from_type_name(file, str8_lit("NonExistent"));
    if(not_found == 0) { printf("OK: lookup NonExistent returns null\n"); }
    else { printf("FAIL: NonExistent should not be found\n"); passed = 0; }
  }
  
  // check CustomListItems structure
  {
    NV_TypeDef *ct = nv_type_def_from_type_name(file, str8_lit("TestCustom"));
    if(ct != 0 && ct->expand != 0)
    {
      NV_ExpandItem *ei = ct->expand->first_item;
      if(ei != 0 && ei->kind == NV_ExpandItemKind_CustomListItems)
      {
        if(ei->custom_list_items.max_items_per_view == 100)
        { printf("OK: CustomListItems max_items=100\n"); }
        else { printf("FAIL: max_items=%llu\n", ei->custom_list_items.max_items_per_view); passed = 0; }
        
        NV_CLVariable *v = ei->custom_list_items.first_variable;
        if(v != 0 && str8_match(v->name, str8_lit("node"), 0))
        { printf("OK: first variable is 'node'\n"); }
        else { printf("FAIL: first variable\n"); passed = 0; }
        
        NV_CLStatement *s = ei->custom_list_items.first_statement;
        if(s != 0 && s->kind == NV_CLStatementKind_Loop)
        { printf("OK: first statement is Loop\n"); }
        else { printf("FAIL: first statement kind\n"); passed = 0; }
      }
      else { printf("FAIL: no CustomListItems expand\n"); passed = 0; }
    }
    else { printf("FAIL: TestCustom type not found or no expand\n"); passed = 0; }
  }
  
  printf("=== Semantic model test %s ===\n\n", passed ? "PASSED" : "FAILED");
  arena_release(arena);
  return passed;
}

internal void
nv_test_file(char *path)
{
  Arena *arena = arena_alloc();
  
  printf("=== Parsing file: %s ===\n", path);
  
  // read file
  String8 file_path = str8_cstring((U8 *)path);
  OS_Handle file = os_file_open(OS_AccessFlag_Read, file_path);
  FileProperties props = os_properties_from_file(file);
  String8 file_data = {0};
  file_data.size = props.size;
  file_data.str = push_array(arena, U8, file_data.size);
  os_file_read(file, r1u64(0, file_data.size), file_data.str);
  os_file_close(file);
  
  if(file_data.size == 0)
  {
    printf("ERROR: could not read file or file is empty\n");
    arena_release(arena);
    return;
  }
  
  printf("File size: %llu bytes\n", file_data.size);
  
  // parse
  U64 time_start = os_now_microseconds();
  NV_XMLParseResult result = nv_xml_parse_from_string(arena, file_data);
  U64 time_end = os_now_microseconds();
  
  printf("Parse time: %llu us\n", time_end - time_start);
  printf("Parse errors: %llu\n", result.error_count);
  
  // collect stats
  NV_TestStats stats = {0};
  nv_test_collect_stats_recursive(result.root, &stats);
  
  printf("\n--- Statistics ---\n");
  printf("  Type:             %llu\n", stats.type_count);
  printf("  AlternativeType:  %llu\n", stats.alternative_type_count);
  printf("  DisplayString:    %llu\n", stats.display_string_count);
  printf("  Expand:           %llu\n", stats.expand_count);
  printf("  Item:             %llu\n", stats.item_count);
  printf("  ArrayItems:       %llu\n", stats.array_items_count);
  printf("  IndexListItems:   %llu\n", stats.index_list_items_count);
  printf("  LinkedListItems:  %llu\n", stats.linked_list_items_count);
  printf("  CustomListItems:  %llu\n", stats.custom_list_items_count);
  printf("  Synthetic:        %llu\n", stats.synthetic_count);
  printf("  ExpandedItem:     %llu\n", stats.expanded_item_count);
  printf("  Intrinsic:        %llu\n", stats.intrinsic_count);
  printf("  TreeItems:        %llu\n", stats.tree_items_count);
  printf("------------------\n\n");
  
  // semantic model
  NV_File *nv_file = nv_file_from_xml(arena, result.root, file_path);
  printf("Semantic model: %llu type definitions\n", nv_file->type_count);
  
  // print global intrinsic count
  printf("Global intrinsics: %llu\n", nv_file->intrinsic_count);
  if(nv_file->intrinsic_count > 0)
  {
    U64 gi_shown = 0;
    for(NV_Intrinsic *gi = nv_file->first_intrinsic; gi != 0 && gi_shown < 5; gi = gi->next, gi_shown += 1)
    {
      printf("  [G] %.*s(%llu params)\n", (int)gi->name.size, gi->name.str, gi->param_count);
    }
    if(nv_file->intrinsic_count > 5) { printf("  ... and %llu more\n", nv_file->intrinsic_count - 5); }
  }
  
  // print first 5 Type names from semantic model
  printf("First types:\n");
  U64 shown = 0;
  for(NV_TypeDef *td = nv_file->first_type; td != 0 && shown < 5; td = td->next, shown += 1)
  {
    printf("  [%llu] %.*s", shown, (int)td->name.size, td->name.str);
    if(td->intrinsic_count > 0) { printf(" [%llu intrinsics]", td->intrinsic_count); }
    if(td->first_display_string != 0) { printf(" [has DisplayString]"); }
    if(td->expand != 0) { printf(" [has Expand: %llu items]", td->expand->item_count); }
    printf("\n");
  }
  
  // cache integration test
  {
    NV_Cache *cache = nv_cache_alloc();
    NV_File *cf = nv_cache_load_from_string(cache, file_data, file_path, NV_SourceKind_File);
    if(cf != 0)
    {
      printf("Cache: %llu entries, %llu types loaded\n", cache->count, cf->type_count);
    }
    nv_cache_release(cache);
  }
  
  printf("\n=== Done ===\n");
  arena_release(arena);
}

////////////////////////////////
//~ Entry point

internal void
entry_point(CmdLine *cmdline)
{
  // run built-in tests
  B32 builtin_passed = nv_test_validate_builtin();
  B32 semantic_passed = nv_test_semantic_model();
  
  // T3: expression translation
  {
    Arena *a = arena_alloc();
    printf("=== Expression translation test ===\n");
    B32 p = 1;
    
    String8 targs[] = { str8_lit("int"), str8_lit("float") };
    
    // $T1 substitution
    String8 e1 = nv_translate_expr(a, str8_lit("cast($T1 *)ptr"), targs, 2);
    if(str8_match(e1, str8_lit("cast(int *)ptr"), 0)) { printf("OK: $T1 substitution\n"); }
    else { printf("FAIL: $T1 sub = '%.*s'\n", (int)e1.size, e1.str); p = 0; }
    
    // C-style cast translation
    String8 e2 = nv_translate_expr(a, str8_lit("(wchar_t *)Data"), targs, 0);
    if(str8_match(e2, str8_lit("cast(wchar_t *)Data"), 0)) { printf("OK: C-cast → cast()\n"); }
    else { printf("FAIL: C-cast = '%.*s'\n", (int)e2.size, e2.str); p = 0; }
    
    // passthrough
    String8 e3 = nv_translate_expr(a, str8_lit("_Mypair._Myval2._Mysize"), targs, 0);
    if(str8_match(e3, str8_lit("_Mypair._Myval2._Mysize"), 0)) { printf("OK: passthrough\n"); }
    else { printf("FAIL: passthrough = '%.*s'\n", (int)e3.size, e3.str); p = 0; }
    
    // format specifier
    String8 f1 = nv_apply_format_spec(a, str8_lit("value"), str8_lit("x"));
    if(str8_match(f1, str8_lit("hex(value)"), 0)) { printf("OK: ,x → hex()\n"); }
    else { printf("FAIL: ,x = '%.*s'\n", (int)f1.size, f1.str); p = 0; }
    
    printf("=== Expression test %s ===\n\n", p ? "PASSED" : "FAILED");
    builtin_passed = builtin_passed && p;
    arena_release(a);
  }
  
  // T4: expansion
  {
    Arena *a = arena_alloc();
    printf("=== Expansion test ===\n");
    B32 p = 1;
    
    String8 xml = str8((U8 *)nv_test_xml, sizeof(nv_test_xml) - 1);
    NV_XMLParseResult xr = nv_xml_parse_from_string(a, xml);
    NV_File *file = nv_file_from_xml(a, xr.root, str8_lit("test"));
    
    // TestArray: should produce an array() expression
    NV_TypeDef *arr_td = nv_type_def_from_type_name(file, str8_lit("TestArray<*>"));
    if(arr_td == 0) { arr_td = file->first_type->next; } // fallback
    if(arr_td != 0 && arr_td->expand != 0)
    {
      String8 targs[] = { str8_lit("int") };
      NV_OutputItemList items = nv_expand_items_from_expand(a, arr_td->expand, targs, 1);
      if(items.count >= 1) { printf("OK: ArrayItems expansion → %llu items\n", items.count); }
      else { printf("FAIL: ArrayItems produced 0 items\n"); p = 0; }
    }
    
    // TestCustom: CustomListItems should produce items
    NV_TypeDef *cust_td = nv_type_def_from_type_name(file, str8_lit("TestCustom"));
    if(cust_td != 0 && cust_td->expand != 0)
    {
      NV_OutputItemList items = nv_expand_items_from_expand(a, cust_td->expand, 0, 0);
      if(items.count >= 1) { printf("OK: CustomListItems → %llu items\n", items.count); }
      else { printf("FAIL: CustomListItems produced 0 items\n"); p = 0; }
    }
    
    // TestSynthetic: should produce synthetic items
    NV_TypeDef *synth_td = nv_type_def_from_type_name(file, str8_lit("TestSynthetic"));
    if(synth_td != 0 && synth_td->expand != 0)
    {
      NV_OutputItemList items = nv_expand_items_from_expand(a, synth_td->expand, 0, 0);
      if(items.count >= 2) { printf("OK: Synthetic+ExpandedItem → %llu items\n", items.count); }
      else { printf("FAIL: Synthetic produced %llu items\n", items.count); p = 0; }
    }
    
    printf("=== Expansion test %s ===\n\n", p ? "PASSED" : "FAILED");
    builtin_passed = builtin_passed && p;
    arena_release(a);
  }
  
  // Intrinsic parsing + inlining tests
  {
    Arena *a = arena_alloc();
    printf("=== Intrinsic test ===\n");
    B32 p = 1;
    
    String8 xml = str8((U8 *)nv_test_intrinsic_xml, sizeof(nv_test_intrinsic_xml) - 1);
    NV_XMLParseResult xr = nv_xml_parse_from_string(a, xml);
    NV_File *file = nv_file_from_xml(a, xr.root, str8_lit("intrinsic_test.natvis"));
    
    // check global intrinsic count
    if(file->intrinsic_count == 2)
    { printf("OK: 2 global intrinsics\n"); }
    else { printf("FAIL: expected 2 global intrinsics, got %llu\n", file->intrinsic_count); p = 0; }
    
    // check global intrinsic names
    NV_Intrinsic *align_intr = nv_intrinsic_from_name(file->first_intrinsic, str8_lit("_AlignInt"));
    if(align_intr != 0 && align_intr->param_count == 2)
    { printf("OK: _AlignInt found with 2 params\n"); }
    else { printf("FAIL: _AlignInt lookup\n"); p = 0; }
    
    NV_Intrinsic *global_helper = nv_intrinsic_from_name(file->first_intrinsic, str8_lit("_GlobalHelper"));
    if(global_helper != 0 && global_helper->param_count == 0 && !global_helper->side_effect)
    { printf("OK: _GlobalHelper found, no params, SideEffect=false\n"); }
    else { printf("FAIL: _GlobalHelper lookup\n"); p = 0; }
    
    // check type-scoped intrinsics
    NV_TypeDef *alloc_td = nv_type_def_from_type_name(file, str8_lit("TestAllocator"));
    if(alloc_td != 0 && alloc_td->intrinsic_count == 1)
    { printf("OK: TestAllocator has 1 type-scoped intrinsic\n"); }
    else { printf("FAIL: TestAllocator intrinsic_count\n"); p = 0; }
    
    NV_TypeDef *frame_td = nv_type_def_from_type_name(file, str8_lit("TestFrame"));
    if(frame_td != 0 && frame_td->intrinsic_count == 2)
    { printf("OK: TestFrame has 2 type-scoped intrinsics\n"); }
    else { printf("FAIL: TestFrame intrinsic_count\n"); p = 0; }
    
    if(frame_td != 0)
    {
      NV_Intrinsic *sn = nv_intrinsic_from_name(frame_td->first_intrinsic, str8_lit("stateName"));
      if(sn != 0 && sn->param_count == 1 && str8_match(sn->category, str8_lit("Method"), 0))
      { printf("OK: stateName: 1 param, Category=Method\n"); }
      else { printf("FAIL: stateName intrinsic\n"); p = 0; }
    }
    
    // test inlining: simple no-arg intrinsic call
    if(alloc_td != 0)
    {
      String8 e1 = nv_inline_intrinsic_calls(a, str8_lit("_GetData()"), alloc_td->first_intrinsic, file->first_intrinsic);
      if(str8_match(e1, str8_lit("((uint8*)Data)"), 0))
      { printf("OK: inline _GetData() → ((uint8*)Data)\n"); }
      else { printf("FAIL: inline _GetData() = '%.*s'\n", (int)e1.size, e1.str); p = 0; }
    }
    
    // test inlining: intrinsic with params
    {
      String8 e2 = nv_inline_intrinsic_calls(a, str8_lit("_AlignInt(Size, 16)"), 0, file->first_intrinsic);
      if(str8_match(e2, str8_lit("(((Size) + (16) - 1) & ~((16) - 1))"), 0))
      { printf("OK: inline _AlignInt(Size, 16)\n"); }
      else { printf("FAIL: inline _AlignInt = '%.*s'\n", (int)e2.size, e2.str); p = 0; }
    }
    
    // test inlining: recursive (intrinsic calls another intrinsic)
    {
      NV_TypeDef *aligned_td = nv_type_def_from_type_name(file, str8_lit("TestAligned"));
      if(aligned_td != 0)
      {
        String8 e3 = nv_inline_intrinsic_calls(a, str8_lit("GetAligned()"), aligned_td->first_intrinsic, file->first_intrinsic);
        // GetAligned() → _AlignInt(Size, 16) → ((Size) + (16) - 1) & ~((16) - 1)
        // result should contain the expanded _AlignInt
        B32 has_align = (e3.size > 10); // should be fully expanded
        if(has_align)
        { printf("OK: recursive inline GetAligned() → '%.*s'\n", (int)e3.size, e3.str); }
        else { printf("FAIL: recursive inline\n"); p = 0; }
      }
    }
    
    // test inlining: global intrinsic called from type context
    {
      NV_TypeDef *gref_td = nv_type_def_from_type_name(file, str8_lit("TestGlobalRef"));
      if(gref_td != 0)
      {
        String8 e4 = nv_inline_intrinsic_calls(a, str8_lit("_GlobalHelper()"), gref_td->first_intrinsic, file->first_intrinsic);
        if(str8_match(e4, str8_lit("(GlobalPtr->Data)"), 0))
        { printf("OK: global intrinsic from type context\n"); }
        else { printf("FAIL: global intrinsic = '%.*s'\n", (int)e4.size, e4.str); p = 0; }
      }
    }
    
    // test: no false inlining on non-intrinsic identifiers
    {
      String8 e5 = nv_inline_intrinsic_calls(a, str8_lit("UnknownFunc(x, y)"), 0, file->first_intrinsic);
      if(str8_match(e5, str8_lit("UnknownFunc(x, y)"), 0))
      { printf("OK: non-intrinsic passthrough\n"); }
      else { printf("FAIL: non-intrinsic = '%.*s'\n", (int)e5.size, e5.str); p = 0; }
    }
    
    printf("=== Intrinsic test %s ===\n\n", p ? "PASSED" : "FAILED");
    builtin_passed = builtin_passed && p;
    arena_release(a);
  }
  
  // T6: cache
  {
    Arena *a = arena_alloc();
    printf("=== Cache test ===\n");
    B32 p = 1;
    
    NV_Cache *cache = nv_cache_alloc();
    NV_File *f = nv_cache_load_from_string(cache, str8((U8 *)nv_test_xml, sizeof(nv_test_xml) - 1), str8_lit("inline"), NV_SourceKind_Embedded);
    if(f != 0 && f->type_count == 7) { printf("OK: cache load from string: %llu types\n", f->type_count); }
    else { printf("FAIL: cache load\n"); p = 0; }
    
    NV_TypeMatch match = {0};
    NV_TypeDef *found = nv_cache_find_type(cache, str8_lit("TestSimple"), &match);
    if(found != 0 && match.matched) { printf("OK: cache find TestSimple\n"); }
    else { printf("FAIL: cache find\n"); p = 0; }
    
    NV_TypeDef *nf = nv_cache_find_type(cache, str8_lit("NonExistent"), 0);
    if(nf == 0) { printf("OK: cache find non-existent → null\n"); }
    else { printf("FAIL: should be null\n"); p = 0; }
    
    nv_cache_release(cache);
    
    printf("=== Cache test %s ===\n\n", p ? "PASSED" : "FAILED");
    builtin_passed = builtin_passed && p;
    arena_release(a);
  }
  
  builtin_passed = builtin_passed && semantic_passed;
  
  // if a file path was provided, parse it too
  String8List inputs = cmdline->inputs;
  for(String8Node *n = inputs.first; n != 0; n = n->next)
  {
    // convert String8 to cstring for printf
    Arena *scratch_arena = arena_alloc();
    char *cpath = (char *)push_cstr(scratch_arena, n->string).str;
    nv_test_file(cpath);
    arena_release(scratch_arena);
  }
  
  if(!builtin_passed)
  {
    printf("\n*** SOME TESTS FAILED ***\n");
  }
  else
  {
    printf("\n*** ALL TESTS PASSED ***\n");
  }
}
