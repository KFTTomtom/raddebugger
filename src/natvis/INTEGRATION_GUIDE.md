# NatVis Integration Guide for RAD Debugger

## Overview

The `natvis/` module provides a complete NatVis parsing and evaluation pipeline.
This guide describes how to wire it into `raddbg_core.c` to enable automatic
type visualization from `.natvis` files.

## Architecture

```
.natvis files
     |
     v
[natvis_parse]    XML → generic tree (NV_XMLNode)
     |
     v
[natvis_types]    Tree → typed model (NV_File, NV_TypeDef, NV_Expand...)
     |
     v
[natvis_eval]     Expression translation (NatVis syntax → RAD eval syntax)
     |
     v
[natvis_expand]   Expand evaluation (ArrayItems, LinkedListItems, CustomListItems...)
     |
     v
[natvis_cache]    File loading, caching, hot-reload
     |
     v
[natvis_integration]  Bridge to E_AutoHookMap / EV_ExpandRuleTable
```

## Files Created

| File | Purpose |
|------|---------|
| `natvis_parse.h/c` | Minimal XML parser, arena-based |
| `natvis_types.h/c` | Typed NatVis model + type pattern matching |
| `natvis_eval.h/c` | Expression bridge ($T1, cast, format specifiers) |
| `natvis_expand.h/c` | Expand item evaluation + CustomListItems interpreter |
| `natvis_cache.h/c` | File cache with hot-reload |
| `natvis_integration.h/c` | RAD Debugger integration (E_AutoHookMap registration) |
| `natvis_inc.h/c` | Module aggregators |
| `natvis_test.c` | Standalone test program |

## Integration Steps in raddbg_core.c

### Step 1: Include the natvis module

In `raddbg_main.c`, add:

```c
#include "natvis/natvis_inc.h"
// ... (in the .c include section)
#include "natvis/natvis_inc.c"
```

Remove `#define NATVIS_STANDALONE` if present (it is NOT defined in raddbg_main,
only in natvis_test.c).

### Step 2: Add NV_State to RD_State

In the `RD_State` struct (raddbg_core.h), add:

```c
NV_State *natvis_state;
B32 use_natvis;
```

### Step 3: Initialize on startup

In `rd_init()` or equivalent:

```c
rd_state->natvis_state = nv_state_alloc();
rd_state->use_natvis = 1;
```

### Step 4: Load .natvis files after modules are loaded

After the "construct default immediate-mode configs based on loaded modules"
section (~line 11906), add:

```c
////////////////////////////
//- natvis: load .natvis files from known locations
//
if(rd_state->use_natvis && rd_state->natvis_state != 0)
{
  // Load from project directory
  // (scan for .natvis files next to the executable being debugged)
  for(U64 module_idx = 0; module_idx < eval_modules_count; module_idx += 1)
  {
    String8 module_path = eval_modules[module_idx].path;
    String8 dir = str8_chop_last_slash(module_path);
    nv_state_load_directory(rd_state->natvis_state, dir);
  }
  
  // Load from user-configured search paths
  // (could be configured via raddbg.mdesk)
  for(String8Node *sp = rd_state->natvis_state->search_paths.first;
      sp != 0; sp = sp->next)
  {
    nv_state_load_directory(rd_state->natvis_state, sp->string);
  }
  
  // Hot-reload check
  nv_check_reload(rd_state->natvis_state);
}
```

### Step 5: Register NatVis auto-hooks

After the existing "add auto-hook rules for type views" section (~line 11952),
add:

```c
////////////////////////////
//- natvis: register NatVis type visualizers as auto-hooks
//
if(rd_state->use_natvis && rd_state->natvis_state != 0)
{
  nv_register_auto_hooks(rd_state->natvis_state, scratch.arena, auto_hook_map);
}
```

This inserts NatVis-derived type patterns into the same auto-hook map that
user-defined `type_view` entries use. NatVis entries are registered **after**
user type_views, so user definitions take priority (first match wins).

### Step 6: Add mdesk configuration

In `raddbg.mdesk`, add a new config key for NatVis:

```
@default_state_val(use_natvis: 1)
```

And optionally a `natvis_path` config for search directories:

```
{
  natvis_path,
  `@collection_commands(add_natvis_path) @row_commands(remove_cfg) x:{path_string}`,
}
```

### Step 7: Cleanup on shutdown

In `rd_release()` or equivalent:

```c
nv_state_release(rd_state->natvis_state);
```

## Type Pattern Translation

NatVis uses `*` for template wildcards. RAD eval uses `?`.
The integration module handles this translation automatically:

| NatVis pattern | RAD eval pattern |
|---------------|------------------|
| `TArray<*>` | `TArray<?>` |
| `std::map<*,*>` | `std::map<?,?>` |
| `FString` | `FString` |

## Expression Translation

| NatVis expression | RAD eval expression |
|------------------|---------------------|
| `(Type *)ptr` | `cast(Type *)ptr` |
| `$T1` | Template arg substitution |
| `{value,x}` | `hex(value)` |
| `member.field` | `member.field` (passthrough) |

## Limitations

1. **Intrinsic**: NatVis `<Intrinsic>` elements (used heavily in Unreal.natvis)
   require debugger-side function calls. Not yet supported.

2. **Runtime condition evaluation**: `Condition` attributes on DisplayString,
   Expand items, and CustomListItems `Break`/`If` require runtime expression
   evaluation. Currently handled with static heuristics.

3. **TreeItems**: Parsed but tree traversal not yet implemented.

4. **IndexListItems**: Parsed but requires runtime index evaluation.

5. **PDB-embedded NatVis**: The cache supports loading from strings
   (`NV_SourceKind_PDB`), but actual PDB stream extraction requires
   integration with the RDI/PDB layer.

## Testing

Build and run the standalone test:

```
cl /Od /DBUILD_DEBUG=1 /DNATVIS_STANDALONE=1 /I..\src\ /I..\local\ natvis_test.c /link /out:natvis_test.exe
natvis_test.exe path\to\file.natvis
```

The test validates:
- XML parsing (entity decoding, attributes, nesting)
- Semantic model (type definitions, display strings, expand items)
- Expression translation ($T substitution, C-cast, format specifiers)
- Expansion (ArrayItems, CustomListItems, Synthetic)
- Type pattern matching (wildcards, nested templates)
- Cache (load, find, hot-reload)

## Performance

| File | Size | Parse Time | Types |
|------|------|-----------|-------|
| base.natvis | 7.5 KB | 63 µs | 21 |
| Unreal.natvis | 291 KB | 1.8 ms | 383 |
| MovieScene.natvis | 27.5 KB | 181 µs | varies |
