// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ NatVis XML Parser — Internal Helpers

typedef struct NV_XMLParseCtx NV_XMLParseCtx;
struct NV_XMLParseCtx
{
  Arena *arena;
  U8 *base;
  U8 *ptr;
  U8 *opl;
  U64 error_count;
};

internal B32
nv_xml__at_end(NV_XMLParseCtx *ctx)
{
  return ctx->ptr >= ctx->opl;
}

internal U8
nv_xml__peek(NV_XMLParseCtx *ctx)
{
  if(ctx->ptr < ctx->opl) { return *ctx->ptr; }
  return 0;
}

internal U8
nv_xml__advance(NV_XMLParseCtx *ctx)
{
  if(ctx->ptr < ctx->opl) { return *ctx->ptr++; }
  return 0;
}

internal void
nv_xml__skip_whitespace(NV_XMLParseCtx *ctx)
{
  while(ctx->ptr < ctx->opl)
  {
    U8 c = *ctx->ptr;
    if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
    {
      ctx->ptr += 1;
    }
    else
    {
      break;
    }
  }
}

internal B32
nv_xml__match_prefix(NV_XMLParseCtx *ctx, char *prefix)
{
  U64 len = MemoryStrlen((U8 *)prefix);
  if((U64)(ctx->opl - ctx->ptr) >= len && MemoryCompare(ctx->ptr, prefix, len) == 0)
  {
    return 1;
  }
  return 0;
}

internal void
nv_xml__skip_prefix(NV_XMLParseCtx *ctx, char *prefix)
{
  U64 len = MemoryStrlen((U8 *)prefix);
  if((U64)(ctx->opl - ctx->ptr) >= len && MemoryCompare(ctx->ptr, prefix, len) == 0)
  {
    ctx->ptr += len;
  }
}

//- rjf: XML entity decoding

internal String8
nv_xml__decode_entities(Arena *arena, String8 raw)
{
  // worst case: no entities, output = input size
  U8 *out = push_array_no_zero(arena, U8, raw.size);
  U64 out_size = 0;
  U64 i = 0;
  while(i < raw.size)
  {
    if(raw.str[i] == '&')
    {
      String8 rest = str8(raw.str + i, raw.size - i);
      if(rest.size >= 4 && MemoryCompare(rest.str, "&lt;", 4) == 0)
      {
        out[out_size++] = '<';
        i += 4;
      }
      else if(rest.size >= 4 && MemoryCompare(rest.str, "&gt;", 4) == 0)
      {
        out[out_size++] = '>';
        i += 4;
      }
      else if(rest.size >= 5 && MemoryCompare(rest.str, "&amp;", 5) == 0)
      {
        out[out_size++] = '&';
        i += 5;
      }
      else if(rest.size >= 6 && MemoryCompare(rest.str, "&quot;", 6) == 0)
      {
        out[out_size++] = '"';
        i += 6;
      }
      else if(rest.size >= 6 && MemoryCompare(rest.str, "&apos;", 6) == 0)
      {
        out[out_size++] = '\'';
        i += 6;
      }
      else
      {
        out[out_size++] = raw.str[i++];
      }
    }
    else
    {
      out[out_size++] = raw.str[i++];
    }
  }
  arena_pop(arena, raw.size - out_size);
  return str8(out, out_size);
}

//- rjf: parse a quoted attribute value

internal String8
nv_xml__parse_attr_value(NV_XMLParseCtx *ctx)
{
  String8 result = str8_zero();
  if(nv_xml__at_end(ctx)) { return result; }
  
  U8 quote_char = nv_xml__peek(ctx);
  if(quote_char != '"' && quote_char != '\'')
  {
    ctx->error_count += 1;
    return result;
  }
  nv_xml__advance(ctx);
  
  U8 *start = ctx->ptr;
  while(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) != quote_char)
  {
    nv_xml__advance(ctx);
  }
  String8 raw = str8(start, (U64)(ctx->ptr - start));
  
  if(!nv_xml__at_end(ctx)) { nv_xml__advance(ctx); } // skip closing quote
  
  result = nv_xml__decode_entities(ctx->arena, raw);
  return result;
}

//- rjf: parse a tag name (letters, digits, underscores, colons, hyphens, dots)

internal String8
nv_xml__parse_name(NV_XMLParseCtx *ctx)
{
  U8 *start = ctx->ptr;
  while(!nv_xml__at_end(ctx))
  {
    U8 c = nv_xml__peek(ctx);
    B32 is_name_char = ((c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '_' || c == ':' || c == '-' || c == '.');
    if(!is_name_char) { break; }
    nv_xml__advance(ctx);
  }
  return str8(start, (U64)(ctx->ptr - start));
}

//- rjf: skip XML comments <!-- ... -->

internal void
nv_xml__skip_comment(NV_XMLParseCtx *ctx)
{
  nv_xml__skip_prefix(ctx, "<!--");
  while(!nv_xml__at_end(ctx))
  {
    if(nv_xml__match_prefix(ctx, "-->"))
    {
      ctx->ptr += 3;
      return;
    }
    nv_xml__advance(ctx);
  }
}

//- rjf: skip <?...?> processing instructions

internal void
nv_xml__skip_processing_instruction(NV_XMLParseCtx *ctx)
{
  nv_xml__skip_prefix(ctx, "<?");
  while(!nv_xml__at_end(ctx))
  {
    if(nv_xml__match_prefix(ctx, "?>"))
    {
      ctx->ptr += 2;
      return;
    }
    nv_xml__advance(ctx);
  }
}

//- rjf: collect text content between tags, trimming leading/trailing whitespace

internal String8
nv_xml__collect_text(NV_XMLParseCtx *ctx)
{
  U8 *start = ctx->ptr;
  while(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) != '<')
  {
    nv_xml__advance(ctx);
  }
  String8 raw = str8(start, (U64)(ctx->ptr - start));
  if(raw.size == 0) { return raw; }
  return nv_xml__decode_entities(ctx->arena, raw);
}

//- rjf: allocate a node

internal NV_XMLNode *
nv_xml__alloc_node(Arena *arena)
{
  NV_XMLNode *n = push_array(arena, NV_XMLNode, 1);
  n->tag = str8_zero();
  n->text = str8_zero();
  return n;
}

//- rjf: parse one element (recursively)

internal NV_XMLNode *
nv_xml__parse_element(NV_XMLParseCtx *ctx)
{
  nv_xml__skip_whitespace(ctx);
  if(nv_xml__at_end(ctx) || nv_xml__peek(ctx) != '<')
  {
    return 0;
  }
  
  // skip '<'
  nv_xml__advance(ctx);
  
  // closing tag? shouldn't be called here, but be safe
  if(nv_xml__peek(ctx) == '/')
  {
    // rewind the '<'
    ctx->ptr -= 1;
    return 0;
  }
  
  NV_XMLNode *node = nv_xml__alloc_node(ctx->arena);
  
  // parse tag name
  node->tag = nv_xml__parse_name(ctx);
  
  // parse attributes
  for(;;)
  {
    nv_xml__skip_whitespace(ctx);
    if(nv_xml__at_end(ctx)) { break; }
    
    U8 c = nv_xml__peek(ctx);
    if(c == '/' || c == '>') { break; }
    
    // parse attr name
    String8 attr_key = nv_xml__parse_name(ctx);
    if(attr_key.size == 0)
    {
      // malformed: skip one char
      nv_xml__advance(ctx);
      ctx->error_count += 1;
      continue;
    }
    
    nv_xml__skip_whitespace(ctx);
    
    String8 attr_val = str8_zero();
    if(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) == '=')
    {
      nv_xml__advance(ctx); // skip '='
      nv_xml__skip_whitespace(ctx);
      attr_val = nv_xml__parse_attr_value(ctx);
    }
    
    NV_XMLAttr *attr = push_array(ctx->arena, NV_XMLAttr, 1);
    attr->key = attr_key;
    attr->value = attr_val;
    SLLQueuePush_N(node->first_attr, node->last_attr, attr, next);
    node->attr_count += 1;
  }
  
  // self-closing tag?
  if(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) == '/')
  {
    nv_xml__advance(ctx); // skip '/'
    if(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) == '>')
    {
      nv_xml__advance(ctx); // skip '>'
    }
    return node;
  }
  
  // skip '>'
  if(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) == '>')
  {
    nv_xml__advance(ctx);
  }
  
  // parse children and text content
  for(;;)
  {
    nv_xml__skip_whitespace(ctx);
    if(nv_xml__at_end(ctx)) { break; }
    
    // check for comments
    if(nv_xml__match_prefix(ctx, "<!--"))
    {
      nv_xml__skip_comment(ctx);
      continue;
    }
    
    // check for closing tag
    if(nv_xml__match_prefix(ctx, "</"))
    {
      ctx->ptr += 2;
      // skip tag name + whitespace + '>'
      nv_xml__parse_name(ctx);
      nv_xml__skip_whitespace(ctx);
      if(!nv_xml__at_end(ctx) && nv_xml__peek(ctx) == '>')
      {
        nv_xml__advance(ctx);
      }
      break;
    }
    
    // check for child element
    if(nv_xml__peek(ctx) == '<')
    {
      // could be processing instruction
      if(nv_xml__match_prefix(ctx, "<?"))
      {
        nv_xml__skip_processing_instruction(ctx);
        continue;
      }
      
      NV_XMLNode *child = nv_xml__parse_element(ctx);
      if(child != 0)
      {
        child->parent = node;
        DLLPushBack(node->first, node->last, child);
      }
      else
      {
        // couldn't parse child — skip one char to avoid infinite loop
        nv_xml__advance(ctx);
      }
      continue;
    }
    
    // collect text content
    String8 text = nv_xml__collect_text(ctx);
    if(text.size > 0)
    {
      if(node->text.size == 0)
      {
        node->text = text;
      }
      else
      {
        // append to existing text (rare: text split by comments)
        String8 combined = push_str8f(ctx->arena, "%S%S", node->text, text);
        node->text = combined;
      }
    }
  }
  
  return node;
}

////////////////////////////////
//~ NatVis XML Parser — Public API

internal NV_XMLParseResult
nv_xml_parse_from_string(Arena *arena, String8 xml_text)
{
  NV_XMLParseResult result = {0};
  
  NV_XMLParseCtx ctx = {0};
  ctx.arena = arena;
  ctx.base = xml_text.str;
  ctx.ptr = xml_text.str;
  ctx.opl = xml_text.str + xml_text.size;
  ctx.error_count = 0;
  
  // handle BOM
  if(xml_text.size >= 3 &&
     xml_text.str[0] == 0xEF &&
     xml_text.str[1] == 0xBB &&
     xml_text.str[2] == 0xBF)
  {
    ctx.ptr += 3;
  }
  
  // create virtual root to hold top-level elements
  NV_XMLNode *root = nv_xml__alloc_node(arena);
  root->tag = str8_lit("__root__");
  
  while(!nv_xml__at_end(&ctx))
  {
    nv_xml__skip_whitespace(&ctx);
    if(nv_xml__at_end(&ctx)) { break; }
    
    // skip processing instructions (<?xml ...?>)
    if(nv_xml__match_prefix(&ctx, "<?"))
    {
      nv_xml__skip_processing_instruction(&ctx);
      continue;
    }
    
    // skip comments
    if(nv_xml__match_prefix(&ctx, "<!--"))
    {
      nv_xml__skip_comment(&ctx);
      continue;
    }
    
    // parse element
    if(nv_xml__peek(&ctx) == '<')
    {
      NV_XMLNode *elem = nv_xml__parse_element(&ctx);
      if(elem != 0)
      {
        elem->parent = root;
        DLLPushBack(root->first, root->last, elem);
      }
      else
      {
        nv_xml__advance(&ctx);
      }
      continue;
    }
    
    // skip stray text outside elements
    nv_xml__advance(&ctx);
  }
  
  result.root = root;
  result.error_count = ctx.error_count;
  return result;
}

////////////////////////////////
//~ NatVis XML Node Helpers

internal NV_XMLNode *
nv_xml_child_from_tag(NV_XMLNode *parent, String8 tag)
{
  if(parent == 0) { return 0; }
  for(NV_XMLNode *child = parent->first; child != 0; child = child->next)
  {
    if(str8_match(child->tag, tag, 0))
    {
      return child;
    }
  }
  return 0;
}

internal NV_XMLNode *
nv_xml_next_from_tag(NV_XMLNode *node, String8 tag)
{
  if(node == 0) { return 0; }
  for(NV_XMLNode *sibling = node->next; sibling != 0; sibling = sibling->next)
  {
    if(str8_match(sibling->tag, tag, 0))
    {
      return sibling;
    }
  }
  return 0;
}

internal String8
nv_xml_attr_from_key(NV_XMLNode *node, String8 key)
{
  if(node == 0) { return str8_zero(); }
  for(NV_XMLAttr *attr = node->first_attr; attr != 0; attr = attr->next)
  {
    if(str8_match(attr->key, key, 0))
    {
      return attr->value;
    }
  }
  return str8_zero();
}

internal B32
nv_xml_attr_exists(NV_XMLNode *node, String8 key)
{
  if(node == 0) { return 0; }
  for(NV_XMLAttr *attr = node->first_attr; attr != 0; attr = attr->next)
  {
    if(str8_match(attr->key, key, 0))
    {
      return 1;
    }
  }
  return 0;
}

internal void
nv_xml_for_each_child_with_tag(NV_XMLNode *parent, String8 tag, NV_XMLIterCallback *cb, void *user_data)
{
  if(parent == 0 || cb == 0) { return; }
  for(NV_XMLNode *child = parent->first; child != 0; child = child->next)
  {
    if(str8_match(child->tag, tag, 0))
    {
      cb(user_data, child);
    }
  }
}

internal U64
nv_xml_child_count_with_tag(NV_XMLNode *parent, String8 tag)
{
  U64 count = 0;
  if(parent == 0) { return 0; }
  for(NV_XMLNode *child = parent->first; child != 0; child = child->next)
  {
    if(str8_match(child->tag, tag, 0))
    {
      count += 1;
    }
  }
  return count;
}
