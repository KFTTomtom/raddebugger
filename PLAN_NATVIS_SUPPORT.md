# Plan d'implémentation : Support NatVis Runtime dans RAD Debugger

> Version 1.0 — 17 avril 2026

---

## Table des matières

1. [Vue d'ensemble](#1-vue-densemble)
2. [Architecture cible](#2-architecture-cible)
3. [Phases d'implémentation](#3-phases-dimplémentation)
4. [Tâches détaillées par agent](#4-tâches-détaillées-par-agent)
5. [Dépendances et ordre d'exécution](#5-dépendances-et-ordre-dexécution)
6. [Critères d'acceptation](#6-critères-dacceptation)
7. [Risques et mitigations](#7-risques-et-mitigations)

---

## 1. Vue d'ensemble

### Objectif

Permettre à RAD Debugger de **charger et interpréter des fichiers NatVis** (`.natvis`) au runtime pour afficher les types C++ de manière lisible dans les watch views, exactement comme Visual Studio le fait.

### Portée

| Inclus | Exclus (v1) |
|--------|-------------|
| Chargement de `.natvis` depuis le projet/utilisateur | `UIVisualizer` (intégration VS-only) |
| Extraction de NatVis embarqué dans les PDB | `Intrinsic` (appels de fonctions spéciales VS) |
| `DisplayString` avec expressions | `TreeItems` (absent des NatVis UE) |
| `Item`, `ExpandedItem`, `Synthetic` | Format specifiers avancés (`hr`, `wm`, `handle`) |
| `ArrayItems` (simple, sans Rank multi-dim) | `ArrayItems` multi-dimensionnel (`Rank` > 1) |
| `IndexListItems` | `MostDerivedType` |
| `LinkedListItems` | `SmartPointer` |
| `CustomListItems` (Loop/Exec/If/Break/Variable) | `HResult` |
| `AlternativeType` | `Version` / module filtering |
| `Condition` sur tous les éléments | `IncludeView` / `ExcludeView` |
| Wildcards template (`$T1`, `$T2`, ...) | `StringView` (v1 — affiche comme DisplayString) |
| Format specifiers de base (`,x`, `,d`, `,s`) | |

### Impact UE estimé

Avec cette portée, on couvre **~80% des types UE** dans `Unreal.natvis` (383 entrées).
Les 20% restants dépendent principalement d'`Intrinsic` (helpers debug-only comme `GDebuggingState`).

---

## 2. Architecture cible

### Nouveau module : `src/natvis/`

```
src/natvis/
├── natvis_parse.h          # Parseur XML → arbre NatVis en mémoire
├── natvis_parse.c
├── natvis_types.h          # Types : NV_Type, NV_DisplayString, NV_Expand, NV_Item, etc.
├── natvis_types.c
├── natvis_eval.h           # Évaluation d'expressions NatVis (bridge vers eval layer)
├── natvis_eval.c
├── natvis_expand.h         # Expansion hooks (EV_ExpandRule) pour les NatVis constructs
├── natvis_expand.c
├── natvis_cache.h          # Cache de NatVis par module/PDB
├── natvis_cache.c
├── natvis_inc.h            # Include agrégateur
├── natvis_inc.c
└── natvis.mdesk            # Métadonnées pour metagen (si nécessaire)
```

### Flux de données

```
                                    ┌─────────────────────┐
                                    │  Fichiers .natvis    │
                                    │  (projet/user/PDB)  │
                                    └──────────┬──────────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │  natvis_parse        │
                                    │  XML → NV_TypeDef[]  │
                                    └──────────┬──────────┘
                                               │
              ┌────────────────────────────────┼────────────────────────────────┐
              │                                │                                │
   ┌──────────▼──────────┐      ┌──────────────▼────────────┐     ┌────────────▼───────────┐
   │  DisplayString       │      │  Simple expansions        │     │  CustomListItems        │
   │  → E_AutoHookMap     │      │  (Item, ArrayItems, etc.) │     │  → NV interpreter       │
   │  (type_view expr)    │      │  → EV_ExpandRule hooks    │     │  → EV_ExpandRule hooks   │
   └──────────┬──────────┘      └──────────────┬────────────┘     └────────────┬───────────┘
              │                                │                                │
              └────────────────────────────────┼────────────────────────────────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │  raddbg_core.c       │
                                    │  (registration       │
                                    │   chaque frame)      │
                                    └──────────┬──────────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │  Watch / Eval views  │
                                    │  (affichage final)   │
                                    └─────────────────────┘
```

### Points d'insertion dans le code existant

| Composant existant | Modification |
|---|---|
| `raddbg.mdesk` (schema) | Ajouter `natvis_file` comme collection avec champ `path` |
| `raddbg_core.c` (frame loop ~11873) | Après le gathering des `type_view`, gatherer aussi les NatVis parsés |
| `ctrl_core.c` (module_open ~3975) | Extraire NatVis depuis PDB en plus de `.raddbg` section |
| `config_core.c` (schema) | Supporter le nouveau type `natvis_file` |
| `eval_visualization_core.c` | Nouveaux `EV_ExpandRule` hooks pour les constructs NatVis |

---

## 3. Phases d'implémentation

### Phase 1 — Fondations (2 semaines)
- Parseur XML minimal
- Types NatVis en mémoire
- Chargement depuis fichiers

### Phase 2 — Expressions et DisplayString (2 semaines)
- Bridge expression NatVis → eval RAD
- DisplayString rendering
- Template wildcards ($T1, $T2)

### Phase 3 — Expansions simples (2 semaines)
- Item, ExpandedItem, Synthetic
- ArrayItems, IndexListItems, LinkedListItems

### Phase 4 — CustomListItems (2 semaines)
- Interpréteur impératif (Variable, Loop, Exec, If, Break)
- Intégration avec EV_ExpandRule

### Phase 5 — Chargement PDB et intégration (1 semaine)
- Extraction NatVis depuis PDB (src/headerblock streams)
- Config utilisateur/projet pour les chemins NatVis
- Priorité et résolution de conflits

### Phase 6 — Polish et tests (1 semaine)
- Tests avec Unreal.natvis complet
- Tests avec stl.natvis de Visual Studio
- Performance et gestion d'erreurs
- Documentation

---

## 4. Tâches détaillées par agent

---

### TÂCHE 1 : Parseur XML minimal pour NatVis

**Agent** : Agent A — "XML Parser"
**Phase** : 1
**Prérequis** : Aucun
**Durée estimée** : 3-4 jours
**Fichiers à créer** : `src/natvis/natvis_parse.h`, `src/natvis/natvis_parse.c`

#### Contexte

Le codebase RAD Debugger n'a aucun parseur XML. NatVis utilise un sous-ensemble prévisible de XML (pas de CDATA complexe, pas de namespaces imbriqués, pas de DTD). Un parseur minimal suffit.

#### Spécifications

1. **Parser les éléments XML** suivants (exhaustif pour NatVis) :
   - Éléments ouvrants avec attributs : `<Type Name="...">`
   - Éléments auto-fermants : `<Break Condition="..."/>`
   - Éléments fermants : `</Type>`
   - Contenu texte entre éléments (expressions dans `<Item>`, `<DisplayString>`, etc.)
   - Commentaires XML : `<!-- ... -->`
   - Déclaration XML : `<?xml ... ?>`

2. **Attributs** : parser clé="valeur" avec guillemets doubles ou simples. Supporter les entités XML standard (`&lt;`, `&gt;`, `&amp;`, `&quot;`, `&apos;`).

3. **API** (style cohérent avec le reste du codebase RAD) :
   ```c
   typedef struct NV_XMLNode NV_XMLNode;
   struct NV_XMLNode
   {
     NV_XMLNode *first;    // premier enfant
     NV_XMLNode *last;     // dernier enfant
     NV_XMLNode *next;     // frère suivant
     NV_XMLNode *prev;     // frère précédent
     NV_XMLNode *parent;
     String8 tag;          // nom de l'élément
     String8 text;         // contenu texte (pour les feuilles)
     NV_XMLAttr *first_attr;
     NV_XMLAttr *last_attr;
   };
   
   typedef struct NV_XMLAttr NV_XMLAttr;
   struct NV_XMLAttr
   {
     NV_XMLAttr *next;
     String8 key;
     String8 value;
   };
   
   // Parse un fichier NatVis XML complet
   internal NV_XMLNode *nv_xml_parse(Arena *arena, String8 xml_text);
   
   // Helpers de navigation
   internal NV_XMLNode *nv_xml_child_from_tag(NV_XMLNode *parent, String8 tag);
   internal String8 nv_xml_attr_from_key(NV_XMLNode *node, String8 key);
   internal NV_XMLNode *nv_xml_next_sibling_from_tag(NV_XMLNode *node, String8 tag);
   ```

4. **Contraintes** :
   - Utiliser les `Arena` du codebase (pas de malloc)
   - Utiliser `String8` pour toutes les chaînes
   - Pas de dépendance externe (pas de libxml, pas d'expat)
   - Tolérant aux erreurs (NatVis mal formé → skip l'élément, pas crash)
   - Performance : parser un fichier de 5000 lignes en < 1ms

#### Tests de validation

- Parser `src/natvis/base.natvis` (244 lignes) sans erreur
- Parser `Engine/Extras/VisualStudioDebugging/Unreal.natvis` (~4500 lignes) sans crash
- Vérifier l'extraction correcte de tous les attributs `Name=`, `Condition=`, `Optional=`
- Vérifier le contenu texte des `<DisplayString>` incluant `{expr}` et `{{` échappés

---

### TÂCHE 2 : Types NatVis en mémoire (modèle sémantique)

**Agent** : Agent B — "NatVis Types"
**Phase** : 1
**Prérequis** : Tâche 1 (API XML définie, pas besoin d'implémentation complète)
**Durée estimée** : 3-4 jours
**Fichiers à créer** : `src/natvis/natvis_types.h`, `src/natvis/natvis_types.c`

#### Contexte

Le parseur XML produit un arbre générique. Cette tâche le convertit en un **modèle typé** spécifique à NatVis, plus facile à consommer par les couches d'évaluation et d'expansion.

#### Spécifications

1. **Types principaux** :
   ```c
   // Un fichier NatVis parsé
   typedef struct NV_File NV_File;
   struct NV_File
   {
     Arena *arena;
     String8 path;           // chemin source du fichier
     NV_TypeDef *first_type; // liste chaînée de définitions
     NV_TypeDef *last_type;
   };
   
   // Une définition <Type> + ses <AlternativeType>
   typedef struct NV_TypeDef NV_TypeDef;
   struct NV_TypeDef
   {
     NV_TypeDef *next;
     String8 name;                    // pattern de type (ex: "TArray<*>")
     String8List alternative_names;   // <AlternativeType>
     NV_Priority priority;            // Low..High
     B32 inheritable;
     NV_DisplayString *first_display_string;
     NV_DisplayString *last_display_string;
     NV_StringView *string_view;      // optionnel
     NV_Expand *expand;               // optionnel
   };
   
   // <DisplayString Condition="...">texte avec {expr}</DisplayString>
   typedef struct NV_DisplayString NV_DisplayString;
   struct NV_DisplayString
   {
     NV_DisplayString *next;
     String8 condition;         // expression condition (vide si inconditionnel)
     NV_DisplayPart *first_part;
     NV_DisplayPart *last_part;
     B32 is_optional;
   };
   
   // Fragment d'un DisplayString : soit texte littéral, soit {expression}
   typedef struct NV_DisplayPart NV_DisplayPart;
   struct NV_DisplayPart
   {
     NV_DisplayPart *next;
     NV_DisplayPartKind kind; // Literal ou Expression
     String8 text;            // texte brut ou expression
     String8 format_spec;     // ",x", ",d", etc. (vide si aucun)
   };
   
   // <Expand>
   typedef struct NV_Expand NV_Expand;
   struct NV_Expand
   {
     B32 hide_raw_view;
     NV_ExpandItem *first_item;
     NV_ExpandItem *last_item;
   };
   
   // Union discriminée pour chaque enfant de <Expand>
   typedef enum NV_ExpandItemKind
   {
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
       NV_CustomListItemsData custom_list_items;
       NV_SyntheticData synthetic;
       NV_ExpandedItemData expanded_item;
     };
   };
   ```

2. **Sous-types pour chaque construct d'expansion** :
   ```c
   // <Item Name="...">expression</Item>
   typedef struct NV_ItemData NV_ItemData;
   struct NV_ItemData
   {
     String8 name;
     String8 expression;
   };
   
   // <ArrayItems>
   typedef struct NV_ArrayItemsData NV_ArrayItemsData;
   struct NV_ArrayItemsData
   {
     String8 size_expr;
     String8 value_pointer_expr;
     String8 lower_bound_expr;    // optionnel
   };
   
   // <IndexListItems>
   typedef struct NV_IndexListItemsData NV_IndexListItemsData;
   struct NV_IndexListItemsData
   {
     String8 size_expr;
     String8 value_node_expr;     // peut contenir $i
   };
   
   // <LinkedListItems>
   typedef struct NV_LinkedListItemsData NV_LinkedListItemsData;
   struct NV_LinkedListItemsData
   {
     String8 size_expr;           // optionnel
     String8 head_pointer_expr;
     String8 next_pointer_expr;
     String8 value_node_expr;
   };
   
   // <CustomListItems>
   typedef struct NV_CustomListItemsData NV_CustomListItemsData;
   struct NV_CustomListItemsData
   {
     U64 max_items_per_view;      // défaut 5000
     NV_CLVariable *first_variable;
     NV_CLVariable *last_variable;
     String8 size_expr;
     NV_CLStatement *first_statement;
     NV_CLStatement *last_statement;
   };
   
   // Variable dans CustomListItems
   typedef struct NV_CLVariable NV_CLVariable;
   struct NV_CLVariable
   {
     NV_CLVariable *next;
     String8 name;
     String8 initial_value;
   };
   
   // Statement dans CustomListItems (Loop, If, Exec, Break, Item)
   typedef enum NV_CLStatementKind
   {
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
     String8 condition;              // pour Loop, If, Elseif, Break conditionnel
     String8 expression;             // pour Exec (assign), Item (value)
     String8 item_name;              // pour Item (display name)
     NV_CLStatement *first_child;    // pour Loop, If, Elseif, Else (body)
     NV_CLStatement *last_child;
   };
   
   // <Synthetic>
   typedef struct NV_SyntheticData NV_SyntheticData;
   struct NV_SyntheticData
   {
     String8 name;
     String8 expression;             // optionnel (pour "Add Watch")
     NV_DisplayString *first_display_string;
     NV_DisplayString *last_display_string;
     NV_Expand *expand;              // optionnel
   };
   
   // <ExpandedItem>
   typedef struct NV_ExpandedItemData NV_ExpandedItemData;
   struct NV_ExpandedItemData
   {
     String8 expression;
   };
   ```

3. **Fonction de conversion** :
   ```c
   // Convertit un arbre XML parsé en modèle NatVis typé
   internal NV_File *nv_file_from_xml(Arena *arena, NV_XMLNode *xml_root, String8 file_path);
   ```

4. **Matching de types** :
   ```c
   // Teste si un nom de type correspond à un pattern NatVis
   // Gère les wildcards * et les bindings $T1, $T2, etc.
   typedef struct NV_TypeMatch NV_TypeMatch;
   struct NV_TypeMatch
   {
     B32 matched;
     String8 template_args[8]; // $T1..$T8
     U64 template_arg_count;
   };
   
   internal NV_TypeMatch nv_type_match(String8 pattern, String8 type_name);
   ```

#### Tests de validation

- Convertir `base.natvis` → vérifier que tous les `<Type>` sont capturés avec les bons attributs
- Convertir `Unreal.natvis` → vérifier les 383 `<Type>` entries, compter les constructs par kind
- Vérifier le parsing de `DisplayString` avec `{expr}` et `{{` → parties Literal/Expression correctes
- Vérifier les `CustomListItems` complexes (ex: `FAttributeStorage`) → structure de statements valide

---

### TÂCHE 3 : Bridge d'expressions NatVis → Eval RAD

**Agent** : Agent C — "Expression Bridge"
**Phase** : 2
**Prérequis** : Tâche 2 (types NatVis), connaissance de `src/eval/`
**Durée estimée** : 4-5 jours
**Fichiers à créer** : `src/natvis/natvis_eval.h`, `src/natvis/natvis_eval.c`

#### Contexte

Les expressions NatVis (dans `<Item>`, `<DisplayString>`, `<Condition>`, etc.) utilisent une syntaxe C++-like qui est **presque mais pas exactement** la même que le langage eval de RAD. Il faut un bridge.

#### Différences entre NatVis et RAD eval

| Feature NatVis | Syntaxe NatVis | Équivalent RAD | Transformation nécessaire |
|---|---|---|---|
| Member access | `a.b`, `a->b` | `a.b`, `a->b` | Aucune |
| Array index | `a[i]` | `a[i]` | Aucune |
| Arithmétique | `+`, `-`, `*`, `/`, `%` | Identique | Aucune |
| Comparaison | `==`, `!=`, `<`, `>`, `<=`, `>=` | Identique | Aucune |
| Logique | `&&`, `\|\|`, `!` | Identique | Aucune |
| C-style cast | `(Type*)expr` | `cast(Type *)expr` | Réécriture syntaxique |
| Ternaire | `a ? b : c` | `a ? b : c` | Aucune |
| `this` | Implicite | `$` (parent ref) | Substitution |
| Template params | `$T1`, `$T2` | `?{T1}`, `?{T2}` | Substitution dans la string |
| Format specifier | `expr,x` | `hex(expr)` | Transformation en lens |
| `$i` (index var) | Utilisé dans IndexList/Loop | Variable locale d'itération | Injection dans le contexte |
| `str,[size]` | `{ptr,su}`, `{ptr,[len]s}` | `array(ptr, len)` | Transformation structurelle |

#### Spécifications

1. **Traducteur d'expressions** :
   ```c
   // Traduit une expression NatVis en expression RAD eval.
   // template_args : bindings $T1..$Tn issus du type matching.
   // context_vars : variables locales (ex: $i pour les index, variables CustomListItems).
   internal String8 nv_translate_expr(
     Arena *arena,
     String8 natvis_expr,
     String8 *template_args,
     U64 template_arg_count,
     NV_EvalContext *ctx     // variables locales, espace de noms
   );
   ```

2. **Évaluation de DisplayString** :
   ```c
   // Évalue un DisplayString complet en parcourant les parties.
   // Retourne la chaîne finale formatée.
   internal String8 nv_eval_display_string(
     Arena *arena,
     NV_DisplayString *ds,
     NV_EvalContext *ctx
   );
   
   // Trouve le premier DisplayString dont la Condition est vraie
   internal NV_DisplayString *nv_select_display_string(
     NV_DisplayString *first,
     NV_EvalContext *ctx
   );
   ```

3. **Évaluation de Condition** :
   ```c
   // Évalue une expression condition NatVis → booléen
   internal B32 nv_eval_condition(
     Arena *arena,
     String8 condition_expr,
     NV_EvalContext *ctx
   );
   ```

4. **Contexte d'évaluation** :
   ```c
   typedef struct NV_EvalContext NV_EvalContext;
   struct NV_EvalContext
   {
     E_Space space;                    // espace mémoire du processus
     E_TypeKey base_type;              // type de l'objet visualisé
     E_IRNode *base_ir;                // IR pour accéder à l'objet
     String8 *template_args;           // $T1..$Tn
     U64 template_arg_count;
     NV_EvalContextVar *first_var;     // variables locales (CustomListItems)
     NV_EvalContextVar *last_var;
   };
   
   typedef struct NV_EvalContextVar NV_EvalContextVar;
   struct NV_EvalContextVar
   {
     NV_EvalContextVar *next;
     String8 name;
     E_Value value;
     E_TypeKey type_key;
   };
   ```

5. **Transformations de format specifiers** :

   | NatVis suffix | Transformation RAD |
   |---|---|
   | `,x` / `,X` | Wrapper `hex(...)` |
   | `,d` | Pas de transformation (défaut) |
   | `,o` | Wrapper `oct(...)` (à ajouter si absent) |
   | `,b` | Wrapper `bin(...)` |
   | `,s` | Interpréter comme `char*` |
   | `,su` | Interpréter comme `wchar_t*` |
   | `,na` | Supprimer l'adresse de l'affichage |
   | `,nd` | Pas de type dynamique |
   | `,en` | Forcer l'affichage enum |
   | `,n` / `,[N]` | `array(ptr, N)` |

#### Tests de validation

- `"_Mypair._Myval2._Mysize"` → identique (pas de transformation)
- `"(wchar_t*)Data.AllocatorInstance.Data"` → `"cast(wchar_t *)Data.AllocatorInstance.Data"`
- `"$T1"` avec binding `["int"]` → `"int"`
- `"{_Mysize} elements"` → évaluation de `_Mysize` comme nombre + concaténation
- `"_Mysize < 16"` comme Condition → évaluation booléenne

---

### TÂCHE 4 : Expansion hooks pour les constructs simples

**Agent** : Agent D — "Simple Expansions"
**Phase** : 3
**Prérequis** : Tâche 2, Tâche 3
**Durée estimée** : 5-6 jours
**Fichiers à créer** : `src/natvis/natvis_expand.h`, `src/natvis/natvis_expand.c`

#### Contexte

Les constructs `Item`, `ExpandedItem`, `Synthetic`, `ArrayItems`, `IndexListItems`, `LinkedListItems` doivent produire des lignes dans le watch view via le système `EV_ExpandRule`.

#### Spécifications

1. **Hook EV_ExpandRule pour NatVis** :
   ```c
   // Hook principal qui dispatche vers le bon expand selon le NV_TypeDef
   EV_EXPAND_RULE_INFO_FUNCTION_DEF(natvis);
   ```

2. **Implémentation par construct** :

   **Item** :
   - Chaque `<Item>` produit une ligne avec `Name` comme label et `expression` évaluée
   - Si `Condition` est présent, évaluer d'abord ; skip si false
   - Si `Optional="true"`, ne pas afficher d'erreur si l'expression échoue

   **ExpandedItem** :
   - Évaluer l'expression comme un objet
   - Injecter les enfants de cet objet comme si c'étaient des enfants directs

   **Synthetic** :
   - Créer un noeud virtuel avec le `Name` donné
   - Si `<DisplayString>` est présent, l'utiliser comme valeur affichée
   - Si `<Expand>` est présent, récurser avec le sous-expand

   **ArrayItems** :
   - Évaluer `Size` → N
   - Évaluer `ValuePointer` → ptr
   - Produire N lignes : `[0]` = `*(ptr+0)`, `[1]` = `*(ptr+1)`, ...
   - Si `LowerBound`, ajuster les index affichés

   **IndexListItems** :
   - Évaluer `Size` → N
   - Pour chaque `i` de 0 à N-1, substituer `$i` dans `ValueNode` et évaluer
   - Produire N lignes : `[0]` = eval(ValueNode avec $i=0), ...

   **LinkedListItems** :
   - Évaluer `HeadPointer` → ptr
   - Boucler : évaluer `ValueNode` dans le contexte du noeud, `NextPointer` pour avancer
   - Si `Size` est présent, l'utiliser comme borne ; sinon, s'arrêter quand ptr == NULL
   - Limite de sécurité : max 10000 éléments (configurable)

3. **Registration** :
   - Pour chaque `NV_TypeDef` chargé, appeler `ev_expand_rule_table_push` avec une clé dérivée du nom de type
   - Stocker le `NV_TypeDef*` dans le `user_data` de l'expand rule

4. **Interaction avec le DisplayString** :
   - Quand un type matche un NatVis, le DisplayString doit remplacer la représentation par défaut dans la cellule "value" du watch
   - Mapper vers `E_AutoHookMap` ou vers un nouveau hook dans la chaîne de stringification `EV_String`

#### Tests de validation

- `TArray<int>` avec 5 éléments → 5 lignes `[0]`..`[4]` avec les valeurs correctes
- `std::vector<float>` via ArrayItems → expansion correcte
- `FString` via DisplayString → affichage de la chaîne, pas de la struct
- `Synthetic` nommé `[Properties]` → noeud virtuel cliquable avec enfants
- `LinkedListItems` sur `String8List` (du codebase RAD) → parcours correct

---

### TÂCHE 5 : Interpréteur CustomListItems

**Agent** : Agent E — "CustomList Interpreter"
**Phase** : 4
**Prérequis** : Tâche 3, Tâche 4
**Durée estimée** : 5-7 jours
**Fichiers à modifier** : `src/natvis/natvis_expand.c` (extension)

#### Contexte

`CustomListItems` est un mini-langage impératif avec variables, boucles, conditions, et émission d'items. C'est le construct le plus complexe de NatVis. Il est utilisé dans Unreal.natvis pour les containers non-triviaux (maps hashées, sparse arrays, attribute storage, etc.).

#### Spécifications

1. **Machine d'exécution** :
   ```c
   typedef struct NV_CLInterpreter NV_CLInterpreter;
   struct NV_CLInterpreter
   {
     Arena *arena;
     NV_EvalContext *eval_ctx;
     
     // Variables locales
     NV_CLRuntimeVar *first_var;
     NV_CLRuntimeVar *last_var;
     
     // Items produits
     NV_CLOutputItem *first_item;
     NV_CLOutputItem *last_item;
     U64 item_count;
     U64 max_items;              // MaxItemsPerView (défaut 5000)
     
     // État
     B32 break_requested;
     U64 total_iterations;       // safety limit
   };
   
   typedef struct NV_CLRuntimeVar NV_CLRuntimeVar;
   struct NV_CLRuntimeVar
   {
     NV_CLRuntimeVar *next;
     String8 name;
     E_Value value;
     E_TypeKey type_key;
   };
   
   typedef struct NV_CLOutputItem NV_CLOutputItem;
   struct NV_CLOutputItem
   {
     NV_CLOutputItem *next;
     String8 name;       // display name
     String8 expression; // expression évaluable pour la valeur
     E_Value value;      // valeur pré-évaluée
     E_TypeKey type_key;
   };
   ```

2. **Exécution des statements** :
   ```c
   // Exécute un bloc de statements CustomListItems
   internal void nv_cl_execute_statements(
     NV_CLInterpreter *interp,
     NV_CLStatement *first
   );
   ```

   **Sémantique par statement kind** :

   | Kind | Comportement |
   |---|---|
   | `Loop` | Tant que `condition` est true (ou indéfini), exécuter le body. Safety limit à 100000 itérations. |
   | `If` | Si condition true → exécuter body. Sinon, vérifier les `Elseif` suivants, puis `Else`. |
   | `Elseif` | Partie d'une chaîne If. Ne s'exécute que si les précédents If/Elseif étaient false. |
   | `Else` | Partie terminale d'une chaîne If. |
   | `Exec` | Évaluer l'expression et assigner le résultat à la variable (syntaxe : `var_name = expr`). |
   | `Break` | Si pas de condition ou condition true → lever `break_requested`. |
   | `Item` | Évaluer l'expression, produire un `NV_CLOutputItem` avec le `Name` donné. Incrémenter `item_count`. Si `item_count >= max_items`, lever `break_requested`. |

3. **Gestion de `Exec`** :
   - Parser la forme `variable = expression`
   - Chercher la variable dans `first_var` par nom
   - Évaluer l'expression (avec les variables courantes disponibles)
   - Mettre à jour la valeur de la variable

4. **Intégration EV_ExpandRule** :
   - L'interpréteur produit une liste de `NV_CLOutputItem`
   - Le hook `EV_ExpandRule` retourne `item_count` comme `row_count`
   - Pour chaque row demandée, retourner l'expression du Nième item

5. **Limites de sécurité** :
   - Max 100000 itérations de loop total (configurable)
   - Max `MaxItemsPerView` items (défaut 5000, attribut XML)
   - Timeout : si l'interprétation prend > 100ms, tronquer et afficher "[truncated]"

#### Tests de validation

- `Unreal.natvis` — `TMap` expansion (si sans Intrinsic, simuler avec un mock)
- `base.natvis` — `RDIM_TypeChunkList` (CustomListItems avec Loop/Exec/Break)
- Boucle infinie dans un NatVis mal formé → s'arrête proprement à la limite
- Variables correctement mises à jour entre les itérations du Loop
- If/Elseif/Else — branchement correct

---

### TÂCHE 6 : Cache NatVis et chargement depuis PDB

**Agent** : Agent F — "NatVis Loading"
**Phase** : 5
**Prérequis** : Tâche 1, Tâche 2
**Durée estimée** : 3-4 jours
**Fichiers à créer** : `src/natvis/natvis_cache.h`, `src/natvis/natvis_cache.c`
**Fichiers à modifier** : `src/ctrl/ctrl_core.c`, `src/pdb/pdb_parse.c` (ou nouveau fichier)

#### Contexte

Les NatVis peuvent provenir de 3 sources :
1. Fichiers `.natvis` référencés dans la config utilisateur/projet
2. Fichiers `.natvis` trouvés dans le dossier du projet
3. NatVis embarqués dans les PDB (via `/src/headerblock` stream)

#### Spécifications

1. **Cache NatVis** :
   ```c
   typedef struct NV_CacheEntry NV_CacheEntry;
   struct NV_CacheEntry
   {
     NV_CacheEntry *next;
     NV_CacheEntry *prev;
     String8 source_path;       // chemin fichier ou "pdb:<path>"
     U64 source_timestamp;      // pour hot-reload
     NV_File *file;             // données parsées
     NV_CacheEntrySource source; // File, PDB, Embedded
   };
   
   typedef struct NV_Cache NV_Cache;
   struct NV_Cache
   {
     Arena *arena;
     NV_CacheEntry *first;
     NV_CacheEntry *last;
     U64 count;
   };
   
   // Init/shutdown
   internal void nv_cache_init(void);
   internal void nv_cache_shutdown(void);
   
   // Charger un fichier .natvis
   internal NV_File *nv_cache_load_file(String8 path);
   
   // Charger les NatVis depuis un PDB
   internal NV_File *nv_cache_load_from_pdb(String8 pdb_path);
   
   // Vérifier si les fichiers ont changé et recharger
   internal void nv_cache_hot_reload_tick(void);
   
   // Obtenir tous les NV_TypeDef actifs (tous fichiers confondus)
   internal NV_TypeDefList nv_cache_all_type_defs(Arena *arena);
   ```

2. **Extraction depuis PDB** :
   - Ouvrir le PDB avec `msf_parsed_from_data` (existant)
   - Lire le stream `/src/headerblock`
   - Pour chaque entrée `PDB_SrcHeaderBlockEntry`, vérifier si `file_path` finit par `.natvis`
   - Lire le stream associé pour obtenir le contenu XML
   - Parser avec `nv_xml_parse` + `nv_file_from_xml`

3. **Hook dans `ctrl_thread__module_open`** :
   - Après la résolution de `initial_debug_info_path`, si c'est un PDB :
   - Extraire les NatVis embarqués
   - Les stocker dans un nouveau champ `natvis_data` sur `CTRL_ModuleImageInfoCacheNode`
   - Remonter vers `raddbg_core.c` via un nouveau `ctrl_natvis_data_from_module`

4. **Config utilisateur/projet** (extension `raddbg.mdesk`) :
   - Ajouter une nouvelle collection `natvis_file` au schema :
     ```
     natvis_file:
     {
       schema_inherited_from: ""
       display: "NatVis File"
       add_cmd: "AddNatvisFile"
       add_schema:
       {
         path: { kind: path, display: "Path" }
       }
     }
     ```
   - Commande `AddNatvisFile` pour ajouter un `.natvis` au projet

5. **Priorité de résolution** (quand plusieurs NatVis matchent le même type) :
   1. Config utilisateur (plus haute priorité)
   2. Config projet
   3. NatVis embarqué dans PDB (du module le plus récemment chargé)
   4. NatVis par défaut (STL/UE built-in existants)

#### Tests de validation

- Charger `Unreal.natvis` depuis un chemin fichier → 383 types chargés
- Charger un PDB contenant du NatVis embarqué → extraction correcte
- Hot-reload : modifier un `.natvis` → les types sont mis à jour dans le watch
- Conflit de priorité : même type dans user + PDB → user gagne

---

### TÂCHE 7 : Intégration dans raddbg_core.c (registration loop)

**Agent** : Agent G — "Integration"
**Phase** : 5
**Prérequis** : Tâches 3, 4, 5, 6
**Durée estimée** : 3-4 jours
**Fichiers à modifier** : `src/raddbg/raddbg_core.c`, `src/raddbg/raddbg.mdesk`, `src/raddbg/raddbg_main.c`

#### Contexte

C'est la tâche d'intégration finale qui branche tout le système NatVis dans la boucle principale de RAD Debugger.

#### Spécifications

1. **Dans la boucle frame** (`raddbg_core.c` ~11873) :
   - Après le bloc `// gather config from loaded modules` existant
   - Ajouter un bloc `// gather natvis from loaded modules and config`
   - Pour chaque module : `ctrl_natvis_data_from_module` → parser si pas en cache
   - Pour les chemins config : `nv_cache_load_file` pour chaque `natvis_file`
   - Collecter tous les `NV_TypeDef`

2. **Registration des type views** :
   - Pour chaque `NV_TypeDef` qui a un `DisplayString` :
     - Convertir en expression eval via `nv_translate_expr`
     - Appeler `e_auto_hook_map_insert_new` (même chemin que les `type_view` existants)
   - Pour chaque `NV_TypeDef` qui a un `Expand` :
     - Appeler `ev_expand_rule_table_push` avec le hook NatVis

3. **Coexistence avec les type_view existants** :
   - Les `type_view` explicites (config utilisateur) ont priorité sur NatVis
   - Les NatVis ont priorité sur les built-in STL/UE
   - Ordre de registration : user type_view → NatVis user/projet → NatVis PDB → built-in

4. **Mettre à jour `raddbg.mdesk`** :
   - Ajouter `natvis_file` à la liste des collections dans la section schema
   - Ajouter la commande `AddNatvisFile`
   - Ajouter un setting booléen `use_natvis_files` (défaut : true)

5. **Mettre à jour `raddbg_main.c`** :
   - Inclure `natvis_inc.c` dans la chaîne d'includes
   - Ajouter `nv_cache_init` / `nv_cache_shutdown` aux points d'init/shutdown

#### Tests de validation

- Lancer RAD Debugger avec un projet qui référence `Unreal.natvis`
- Vérifier que `TArray<int>` s'affiche correctement dans le watch
- Vérifier que `FString` affiche la chaîne, pas la struct
- Vérifier que les type_view explicites prennent le dessus sur NatVis
- Vérifier que le toggle `use_natvis_files` désactive bien le support NatVis
- Hot-reload : modifier le `.natvis` pendant le debug → mise à jour

---

### TÂCHE 8 : Tests et polish

**Agent** : Agent H — "QA et Tests"
**Phase** : 6
**Prérequis** : Tâche 7
**Durée estimée** : 3-5 jours

#### Spécifications

1. **Tests avec Unreal.natvis** :
   - Debugger un projet UE5 réel
   - Vérifier les types les plus courants :
     - `FString` → affichage chaîne ✓
     - `FName` → affichage nom (avec/sans Intrinsic) ✓/⚠
     - `TArray<T>` → expansion array ✓
     - `TMap<K,V>` → expansion map (avec CustomListItems) ✓
     - `TSet<T>` → expansion set ✓
     - `TSharedPtr<T>` / `TSharedRef<T>` → deref ✓
     - `TObjectPtr<T>` → deref ✓
     - `FVector` / `FRotator` / `FTransform` → affichage composantes ✓
     - `UObject*` → affichage nom de classe ⚠ (dépend d'Intrinsic)
     - `FGameplayTag` → affichage tag ✓
   - Documenter quels types fonctionnent et lesquels nécessitent Intrinsic

2. **Tests avec stl.natvis (VS)** :
   - `std::vector<T>` ✓
   - `std::string` ✓
   - `std::map<K,V>` (utilise TreeItems → hors scope v1) ⚠
   - `std::unordered_map<K,V>` ✓
   - `std::shared_ptr<T>` ✓
   - `std::unique_ptr<T>` ✓
   - `std::optional<T>` ✓

3. **Tests de robustesse** :
   - NatVis malformé (XML invalide) → erreur gracieuse, pas de crash
   - Expression NatVis invalide → "[error: ...]" dans la cellule, pas de crash
   - Type qui matche plusieurs NatVis → priorité respectée
   - Gros fichier NatVis (10000+ lignes) → performance acceptable (< 10ms parse)
   - CustomListItems avec boucle quasi-infinie → s'arrête à la limite

4. **Performance** :
   - Mesurer l'impact sur le frame time avec 0, 1, 10, 50 NatVis files chargés
   - Le matching de type ne doit pas ralentir les frames (cache de résolution)
   - Le parsing ne doit se faire qu'une fois (+ hot-reload)

5. **Documentation** :
   - Ajouter une section dans le README build pour les NatVis
   - Documenter les constructs supportés vs non-supportés
   - Documenter comment ajouter un `.natvis` au projet

---

## 5. Dépendances et ordre d'exécution

```
                    TÂCHE 1 (XML Parser)
                    ┌───────┴──────┐
                    │              │
              TÂCHE 2 (Types)   TÂCHE 6 (Cache/PDB) ← dépend aussi de T2
                    │
              TÂCHE 3 (Expr Bridge)
                    │
         ┌─────────┼──────────┐
         │                    │
   TÂCHE 4 (Simple Exp)  TÂCHE 5 (CustomList) ← dépend aussi de T4
         │                    │
         └─────────┬──────────┘
                   │
             TÂCHE 7 (Integration) ← dépend aussi de T6
                   │
             TÂCHE 8 (Tests/Polish)
```

### Parallélisation possible

| Agents en parallèle | Tâches |
|---|---|
| **Sprint 1** | T1 (XML Parser) seul — ou en parallèle avec T2 si l'API XML est définie d'abord |
| **Sprint 2** | T3 (Expr Bridge) + T6 (Cache/PDB) en parallèle |
| **Sprint 3** | T4 (Simple Exp) + T5 (CustomList) en parallèle (T5 démarre après les bases de T4) |
| **Sprint 4** | T7 (Integration) puis T8 (Tests) |

### Timeline optimiste (avec parallélisation)

| Semaine | Activité |
|---|---|
| S1 | T1 + T2 |
| S2 | T3 + T6 |
| S3 | T4 + début T5 |
| S4 | T5 (fin) + T7 |
| S5 | T8 |

**Total : ~5 semaines** avec 2 agents en parallèle.

---

## 6. Critères d'acceptation

### Minimum Viable (Phase 3 complète)

- [ ] Charger un fichier `.natvis` depuis la config projet
- [ ] `DisplayString` fonctionne pour les types simples
- [ ] `Item`, `ArrayItems`, `IndexListItems`, `LinkedListItems` fonctionnent
- [ ] `TArray<T>`, `FString`, `std::vector<T>`, `std::string` s'affichent correctement
- [ ] Pas de régression sur les type_view existants

### Feature Complete (Phase 5 complète)

- [ ] `CustomListItems` fonctionne (Loop, Exec, If, Break)
- [ ] `Synthetic` et `ExpandedItem` fonctionnent
- [ ] NatVis embarqué dans PDB extrait automatiquement
- [ ] Hot-reload des fichiers `.natvis`
- [ ] Priorité de résolution correcte (user > projet > PDB > built-in)
- [ ] Setting `use_natvis_files` pour désactiver

### Production Ready (Phase 6 complète)

- [ ] `Unreal.natvis` charge et 80%+ des types fonctionnent
- [ ] `stl.natvis` (VS) charge et les types principaux fonctionnent
- [ ] Performance : < 1ms overhead par frame avec NatVis actifs
- [ ] Robustesse : aucun crash sur NatVis malformé
- [ ] Documentation à jour

---

## 7. Risques et mitigations

| Risque | Impact | Probabilité | Mitigation |
|---|---|---|---|
| **Les `Intrinsic` sont requis pour beaucoup de types UE** | ~20% des types UE (FName, UObject) ne fonctionneront pas sans eux | Haute | Documenter clairement. Implémenter les Intrinsic les plus courants dans une phase ultérieure. Garder les type_view built-in comme fallback. |
| **Les expressions NatVis sont plus permissives que l'eval RAD** | Certaines expressions ne parseront pas | Moyenne | Le bridge (T3) doit gérer les cas connus. Logger les expressions non supportées sans crasher. |
| **Performance du matching de type à chaque frame** | Ralentissement avec beaucoup de types | Moyenne | Cache de résolution type→NatVis. Ne re-matcher que quand les NatVis changent. |
| **Le modèle `EV_ExpandRule` ne couvre pas tous les cas NatVis** | Synthetic et ExpandedItem nécessitent peut-être des extensions au modèle | Moyenne | Prévoir des extensions à `EV_Block` dans T4. |
| **Hot-reload complexe** | Race conditions avec le thread de debug | Faible | Utiliser le même pattern que le hot-reload de fichiers existant (file_stream). |
| **Conflits avec les type_view built-in UE** | Double visualisation | Faible | Ordre de priorité strict. Les NatVis remplacent les built-in quand activés. |

---

_Ce plan est prêt à être distribué à des agents d'implémentation. Chaque tâche est auto-contenue avec contexte, spécifications, et critères de validation._
