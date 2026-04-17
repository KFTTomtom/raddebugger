// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef NATVIS_PARSE_H
#define NATVIS_PARSE_H

////////////////////////////////
//~ NatVis XML Parser — Types

typedef struct NV_XMLAttr NV_XMLAttr;
struct NV_XMLAttr
{
  NV_XMLAttr *next;
  String8 key;
  String8 value;
};

typedef struct NV_XMLNode NV_XMLNode;
struct NV_XMLNode
{
  NV_XMLNode *first;
  NV_XMLNode *last;
  NV_XMLNode *next;
  NV_XMLNode *prev;
  NV_XMLNode *parent;
  String8 tag;
  String8 text;
  NV_XMLAttr *first_attr;
  NV_XMLAttr *last_attr;
  U64 attr_count;
};

typedef struct NV_XMLParseResult NV_XMLParseResult;
struct NV_XMLParseResult
{
  NV_XMLNode *root;
  U64 error_count;
};

////////////////////////////////
//~ NatVis XML Parser — Functions

internal NV_XMLParseResult nv_xml_parse_from_string(Arena *arena, String8 xml_text);

////////////////////////////////
//~ NatVis XML Node Helpers

internal NV_XMLNode *nv_xml_child_from_tag(NV_XMLNode *parent, String8 tag);
internal NV_XMLNode *nv_xml_next_from_tag(NV_XMLNode *node, String8 tag);
internal String8     nv_xml_attr_from_key(NV_XMLNode *node, String8 key);
internal B32         nv_xml_attr_exists(NV_XMLNode *node, String8 key);

typedef void NV_XMLIterCallback(void *user_data, NV_XMLNode *node);
internal void nv_xml_for_each_child_with_tag(NV_XMLNode *parent, String8 tag, NV_XMLIterCallback *cb, void *user_data);
internal U64  nv_xml_child_count_with_tag(NV_XMLNode *parent, String8 tag);

#endif // NATVIS_PARSE_H
