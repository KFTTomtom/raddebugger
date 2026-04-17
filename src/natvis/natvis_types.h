// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_TYPES_H
#define NATVIS_TYPES_H

////////////////////////////////
//~ NatVis Priority

typedef enum NV_Priority
{
  NV_Priority_Low,
  NV_Priority_MediumLow,
  NV_Priority_Medium,
  NV_Priority_MediumHigh,
  NV_Priority_High,
  NV_Priority_COUNT,
} NV_Priority;

////////////////////////////////
//~ NatVis DisplayString

typedef enum NV_DisplayPartKind
{
  NV_DisplayPartKind_Literal,
  NV_DisplayPartKind_Expression,
  NV_DisplayPartKind_COUNT,
} NV_DisplayPartKind;

typedef struct NV_DisplayPart NV_DisplayPart;
struct NV_DisplayPart
{
  NV_DisplayPart *next;
  NV_DisplayPartKind kind;
  String8 text;
  String8 format_spec;
};

typedef struct NV_DisplayString NV_DisplayString;
struct NV_DisplayString
{
  NV_DisplayString *next;
  String8 condition;
  NV_DisplayPart *first_part;
  NV_DisplayPart *last_part;
  B32 is_optional;
};

////////////////////////////////
//~ NatVis StringView

typedef struct NV_StringView NV_StringView;
struct NV_StringView
{
  NV_StringView *next;
  String8 expression;
  String8 condition;
};

////////////////////////////////
//~ NatVis Expand Item Kinds

typedef enum NV_ExpandItemKind
{
  NV_ExpandItemKind_Nil,
  NV_ExpandItemKind_Item,
  NV_ExpandItemKind_ArrayItems,
  NV_ExpandItemKind_IndexListItems,
  NV_ExpandItemKind_LinkedListItems,
  NV_ExpandItemKind_TreeItems,
  NV_ExpandItemKind_CustomListItems,
  NV_ExpandItemKind_Synthetic,
  NV_ExpandItemKind_ExpandedItem,
  NV_ExpandItemKind_COUNT,
} NV_ExpandItemKind;

////////////////////////////////
//~ NatVis Item

typedef struct NV_ItemData NV_ItemData;
struct NV_ItemData
{
  String8 name;
  String8 expression;
};

////////////////////////////////
//~ NatVis ArrayItems

typedef struct NV_ArrayItemsData NV_ArrayItemsData;
struct NV_ArrayItemsData
{
  String8 size_expr;
  String8 value_pointer_expr;
  String8 lower_bound_expr;
};

////////////////////////////////
//~ NatVis IndexListItems

typedef struct NV_IndexListItemsData NV_IndexListItemsData;
struct NV_IndexListItemsData
{
  String8 size_expr;
  String8 value_node_expr;
};

////////////////////////////////
//~ NatVis LinkedListItems

typedef struct NV_LinkedListItemsData NV_LinkedListItemsData;
struct NV_LinkedListItemsData
{
  String8 size_expr;
  String8 head_pointer_expr;
  String8 next_pointer_expr;
  String8 value_node_expr;
};

////////////////////////////////
//~ NatVis TreeItems

typedef struct NV_TreeItemsData NV_TreeItemsData;
struct NV_TreeItemsData
{
  String8 size_expr;
  String8 head_pointer_expr;
  String8 left_pointer_expr;
  String8 right_pointer_expr;
  String8 value_node_expr;
};

////////////////////////////////
//~ NatVis CustomListItems — statements

typedef enum NV_CLStatementKind
{
  NV_CLStatementKind_Nil,
  NV_CLStatementKind_Loop,
  NV_CLStatementKind_If,
  NV_CLStatementKind_Elseif,
  NV_CLStatementKind_Else,
  NV_CLStatementKind_Exec,
  NV_CLStatementKind_Break,
  NV_CLStatementKind_Item,
  NV_CLStatementKind_COUNT,
} NV_CLStatementKind;

typedef struct NV_CLStatement NV_CLStatement;
struct NV_CLStatement
{
  NV_CLStatement *next;
  NV_CLStatementKind kind;
  String8 condition;
  String8 expression;
  String8 item_name;
  NV_CLStatement *first_child;
  NV_CLStatement *last_child;
};

typedef struct NV_CLVariable NV_CLVariable;
struct NV_CLVariable
{
  NV_CLVariable *next;
  String8 name;
  String8 initial_value;
};

typedef struct NV_CustomListItemsData NV_CustomListItemsData;
struct NV_CustomListItemsData
{
  U64 max_items_per_view;
  NV_CLVariable *first_variable;
  NV_CLVariable *last_variable;
  String8 size_expr;
  NV_CLStatement *first_statement;
  NV_CLStatement *last_statement;
};

////////////////////////////////
//~ NatVis Synthetic

typedef struct NV_Expand NV_Expand;

typedef struct NV_SyntheticData NV_SyntheticData;
struct NV_SyntheticData
{
  String8 name;
  String8 watch_expression;
  NV_DisplayString *first_display_string;
  NV_DisplayString *last_display_string;
  NV_Expand *expand;
};

////////////////////////////////
//~ NatVis ExpandedItem

typedef struct NV_ExpandedItemData NV_ExpandedItemData;
struct NV_ExpandedItemData
{
  String8 expression;
};

////////////////////////////////
//~ NatVis ExpandItem (union of all expand children)

typedef struct NV_ExpandItem NV_ExpandItem;
struct NV_ExpandItem
{
  NV_ExpandItem *next;
  NV_ExpandItemKind kind;
  String8 condition;
  B32 is_optional;
  union
  {
    NV_ItemData item;
    NV_ArrayItemsData array_items;
    NV_IndexListItemsData index_list_items;
    NV_LinkedListItemsData linked_list_items;
    NV_TreeItemsData tree_items;
    NV_CustomListItemsData custom_list_items;
    NV_SyntheticData synthetic;
    NV_ExpandedItemData expanded_item;
  };
};

////////////////////////////////
//~ NatVis Expand

struct NV_Expand
{
  B32 hide_raw_view;
  NV_ExpandItem *first_item;
  NV_ExpandItem *last_item;
  U64 item_count;
};

////////////////////////////////
//~ NatVis TypeDef (one <Type> entry)

typedef struct NV_TypeDef NV_TypeDef;
struct NV_TypeDef
{
  NV_TypeDef *next;
  String8 name;
  String8List alternative_names;
  NV_Priority priority;
  B32 inheritable;
  NV_DisplayString *first_display_string;
  NV_DisplayString *last_display_string;
  NV_StringView *string_view;
  NV_Expand *expand;
};

////////////////////////////////
//~ NatVis File (parsed file)

typedef struct NV_File NV_File;
struct NV_File
{
  Arena *arena;
  String8 path;
  NV_TypeDef *first_type;
  NV_TypeDef *last_type;
  U64 type_count;
};

////////////////////////////////
//~ NatVis Type Matching

typedef struct NV_TypeMatch NV_TypeMatch;
struct NV_TypeMatch
{
  B32 matched;
  String8 template_args[16];
  U64 template_arg_count;
};

////////////////////////////////
//~ NatVis Conversion & Matching Functions

internal NV_File *     nv_file_from_xml(Arena *arena, NV_XMLNode *xml_root, String8 file_path);
internal NV_TypeMatch  nv_type_match(String8 pattern, String8 type_name);
internal NV_TypeDef *  nv_type_def_from_type_name(NV_File *file, String8 type_name);

#endif // NATVIS_TYPES_H
