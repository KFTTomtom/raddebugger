# RAD Debugger - Analyse Complète du Codebase

> Document généré le 17 avril 2026 — analyse du repository `D:\Tools\raddebugger`

---

## Table des matières

1. [Vue d'ensemble](#1-vue-densemble)
2. [Architecture du code source](#2-architecture-du-code-source)
3. [Système de build](#3-système-de-build)
4. [Couche UI et ergonomie](#4-couche-ui-et-ergonomie)
5. [Système de configuration et persistance](#5-système-de-configuration-et-persistance)
6. [Intégration et interopérabilité](#6-intégration-et-interopérabilité)
7. [Points forts](#7-points-forts)
8. [Limitations et axes d'amélioration](#8-limitations-et-axes-damélioration)
9. [Pistes d'intégration avec Cursor](#9-pistes-dintégration-avec-cursor)

---

## 1. Vue d'ensemble

**RAD Debugger** est un debugger natif, user-mode, multi-process, graphique, développé par **Epic Games Tools** sous licence **MIT**. Il cible actuellement le debugging local **Windows x64** avec **PDB**, avec un port Linux en cours.

Le projet comprend trois outils :
- **raddbg** — le debugger graphique principal
- **radlink** — un linker haute performance pour PE/COFF (50% plus rapide sur les gros projets)
- **radbin** — utilitaire CLI pour inspection/conversion de fichiers binaires et debug info

### Statut
- **Alpha** (v0.9.25 au moment de l'analyse)
- Debug local Windows x64 + PDB : fonctionnel
- Debug local Linux x64 + DWARF : en cours de développement
- Debug distant : planifié mais non implémenté

### Technologies
- **Langage principal** : C (avec C++ limité aux tests/mules)
- **Build** : scripts batch/shell custom (pas de CMake/Makefile)
- **UI** : framework immediate-mode custom (pas ImGui)
- **Rendu** : D3D11 (Windows), OpenGL (Linux)
- **Fonts** : DirectWrite (Windows), FreeType (Linux prévu)
- **Métaprogrammation** : Metadesk (`.mdesk`) + metagen

---

## 2. Architecture du code source

### Structure des répertoires

```
raddebugger/
├── data/           # Assets embarqués (icône, etc.)
├── src/            # Tout le code source (~52 sous-dossiers)
├── build/          # Artéfacts de build (non versionné)
├── local/          # Config locale (non versionné)
├── build.bat       # Script de build Windows
├── build.sh        # Script de build Linux
├── README.md       # Documentation technique
└── CHANGELOG.md    # Notes de version
```

### Organisation en couches (layers)

Le code est organisé en **couches à dépendances acycliques**. Chaque couche correspond à un dossier dans `src/` et à un namespace C (préfixe court + `_`).

#### Fondation
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `base` | _(aucun)_ | Arènes mémoire, strings, hashing, threads, CLI, macros universelles |
| `os/core` | `OS_` | Abstraction OS non-graphique (mémoire, threads, fichiers) |
| `os/gfx` | `OS_` | Abstraction OS graphique (fenêtres, input) |

#### Formats binaires et debug info
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `coff` | `COFF_` | Parsing/écriture COFF |
| `pe` | `PE_` | Parsing/écriture PE |
| `elf` | `ELF_` | Parsing ELF |
| `msf` | `MSF_` | Container MSF (PDB) |
| `pdb` | `PDB_` | Parsing/écriture PDB |
| `codeview` | `CV_` | Records CodeView (types/symbols) |
| `dwarf` | `DW_` | Parsing DWARF 2–5 |
| `lib_rdi` | `RDI_` | Format RDI (RAD Debug Info) — lib standalone |

#### Conversion vers RDI (format interne)
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `rdi_from_pdb` | `P2R_` | PDB → RDI |
| `rdi_from_dwarf` | `D2R_` | DWARF → RDI (en cours) |
| `rdi_from_elf` | `E2R_` | ELF → RDI |
| `rdi_from_coff` | `C2R_` | COFF → RDI |

#### Moteur de debug
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `demon` | `DMN_` | Contrôle bas-niveau process (Win32 debug API / ptrace) |
| `ctrl` | `CTRL_` | Breakpoints, stepping, contrôle asynchrone |
| `dbg_engine` | `D_` | Logique debugger de haut niveau |
| `dbg_info` | `DI_` | Cache de debug info (RDI parsé), chargement async |
| `eval` | `E_` | Compilateur d'expressions (lexer → parser → typechecker → IR → eval) |
| `eval_visualization` | `EV_` | Visualisation des évaluations (watch tables, lenses) |

#### Présentation / UI
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `render` | `R_` | Abstraction GPU (D3D11/OpenGL) |
| `draw` | `DR_` | API de dessin 2D haut-niveau |
| `font_cache` | `FNT_` | Cache de glyphes rastérisés |
| `font_provider` | `FP_` | Abstraction backend font |
| `ui` | `UI_` | Framework UI immediate-mode custom |
| `raddbg` | `RD_` | Application debugger (commandes, vues, panels, config) |

#### NatVis (visualisation de types)
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `natvis` | `NV_` | Parsing XML, modèle typé, traduction d'expressions, expansion, cache hot-reload, intégration auto-hooks |

Fichiers : `natvis_parse`, `natvis_types`, `natvis_eval`, `natvis_expand`, `natvis_cache`, `natvis_integration`, `natvis_inc`, `natvis_test`

#### Services et support
| Couche | Namespace | Rôle |
|--------|-----------|------|
| `artifact_cache` | `AC_` | Cache async d'artéfacts avec éviction |
| `file_stream` | `FS_` | Streaming fichier asynchrone avec hot-reload |
| `content` | `C_` | Store de données par hash 128-bit |
| `text` | `TXT_` | Traitement texte, tokenisation source |
| `mdesk` | `MD_` | Parseur de fichiers Metadesk |
| `config` | `CFG_` | Arbre de configuration structuré |

### Point d'entrée

Le build est **unity-style** : `raddbg_main.c` inclut (`#include`) toute la pile de `base` jusqu'à `raddbg`. Le flux est :

1. `wmain` / `wWinMain` (OS-specific) → `main_thread_base_entry_point()`
2. Init de toutes les couches (di, ctrl, render, dbg_engine, raddbg...)
3. Appel de `entry_point(&cmdline)` (défini par chaque programme)
4. Boucle principale : frame update → UI → render → autosave

---

## 3. Système de build

### Caractéristiques
- **Pas de CMake/Makefile** — scripts `build.bat` (Windows) et `build.sh` (Linux)
- **Unity build** — un seul fichier `.c` par cible (ex: `raddbg_main.c`)
- **Metagen** — pré-étape qui parse les `.mdesk` et génère du code C
- **Compilateurs** : MSVC ou Clang (Windows), Clang ou GCC (Linux)

### Cibles de build
```
build raddbg          # Debugger graphique
build radlink         # Linker
build radbin          # Utilitaire binaire CLI
build raddump         # Dump tool
build mule_main       # Exécutable de test
build torture         # Tests automatisés
```

### Options
```
build raddbg release  # Build optimisé
build raddbg clang    # Avec Clang au lieu de MSVC
build raddbg asan     # Address Sanitizer
build raddbg spall    # Profilage Spall
```

### CI
GitHub Actions (`builds.yml`) : matrice MSVC×Clang × debug×release pour `raddbg`, `radlink`, `radbin`, `mule_main` + job torture avec ASAN.

---

## 4. Couche UI et ergonomie

### Framework UI

RAD Debugger utilise un **framework UI immediate-mode entièrement custom** :
- Arbre de boîtes hiérarchique (`UI_Box`)
- Système d'événements, focus, scrolling
- Widgets de base (line edit, scroll bars, labels)
- Résolution de thèmes par tags

### Système de panels et vues

Le layout est un **arbre de splits** (`CFG_PanelNode`) :
- Chaque noeud a un `split_axis` et `pct_of_parent`
- Les feuilles contiennent des **onglets** (tabs)
- Les onglets sont soit des **watch tables** (paramétrées par query), soit des **vues nommées**

#### Onglets "watch table" (query-backed)
| Nom | Query/Type |
|-----|------------|
| Watch | Expression utilisateur |
| Locals | Variables locales |
| Registers | Registres |
| Globals | Symboles globaux |
| Call Stack | Pile d'appels |
| Targets | Cibles de debug |
| Breakpoints | Points d'arrêt |
| Threads | Threads |
| Modules | Modules chargés |
| Debug Info | Info de debug |

#### Onglets "vue" (view hooks)
| Nom | Fonction |
|-----|----------|
| Text | Visualisation source |
| Disasm | Désassemblage |
| Memory | Inspecteur mémoire |
| Output | Sortie/logs |
| Bitmap | Visualisation image |
| Color | Visualisation couleur |
| Geo3D | Géométrie 3D |

### Thèmes

- Système de patterns : ensemble de tags → couleur RGBA linéaire
- Thème par défaut + thèmes utilisateur/projet
- Commandes : `edit_user_theme`, `fork_theme`, `save_theme`, `use_project_theme`
- Cache LRU pour la résolution des couleurs

### Raccourcis clavier

- Stockés dans le CFG utilisateur sous `keybindings`
- Table par défaut générée dans `raddbg.meta.c` (combinaisons `OS_Key_*` + modifiers)
- UI de rebind intégrée
- Système de commandes (`RD_CmdKind_*`) : tous les raccourcis sont des commandes nommées

### Code source

- Affichage via `rd_code_view_build` : numéros de ligne, tab width configurable, font code dédiée
- Navigation par commandes : goto line, recherche, navigation stack/thread
- Breakpoints et watch pins affichés en marge
- Remapping de chemins source (`rd_possible_overrides_from_file_path`)

### Polices

- 3 slots de fontes : main, code, icons
- Cache de glyphes GPU (atlas de textures)
- Subpixel rendering
- Ajustement DPI

### Visualisation de types

- **Support NatVis runtime** : module complet `src/natvis/` (parsing XML, modèle typé, traduction d'expressions, expansion, cache avec hot-reload)
- Intégration via `E_AutoHookMap` : les NatVis sont enregistrés comme auto-hooks type_view dans la frame loop de `raddbg_core.c`
- Chargement automatique des `.natvis` depuis les répertoires des modules chargés, détection UE (Extras/VisualStudioDebugging), et chemins configurés (`natvis_path`)
- Setting `use_natvis` (défaut : activé) dans `raddbg.mdesk`
- Système propre : `type_view` (pattern + expression dans la config) — prioritaire sur NatVis
- Visualisateurs intégrés pour STL et UE (activables via settings)
- Markup embeddable dans les binaires debuggés (`lib_raddbg_markup`)
- Lenses : text, disasm, memory, bitmap, color, geo3d

#### Limitations NatVis actuelles

| Limitation | Détail |
|------------|--------|
| `Intrinsic` | Non supporté (nécessaire pour ~20% des types UE : FName, UObject) |
| Conditions runtime | Heuristiques statiques, pas d'évaluation réelle |
| `TreeItems` | Parsé mais traversal non implémenté |
| `IndexListItems` | Parsé, évaluation index runtime partielle |
| PDB-embedded NatVis | API prête (`NV_SourceKind_PDB`), extraction PDB non branchée |
| `EV_ExpandRule` hooks | Non utilisés — l'expansion passe par auto-hooks, pas par la table d'expand rules |

---

## 5. Système de configuration et persistance

### Format

- Fichiers texte propriétaire (syntaxe Metadesk-like)
- Ligne magique : `// raddbg user` ou `// raddbg project`
- Arbre structuré sérialisé par `cfg_string_from_tree`
- **Pas JSON, pas TOML, pas INI**

### Buckets de config

| Bucket | Contenu |
|--------|---------|
| `user` | Settings globaux, keybindings, thèmes, layout fenêtres |
| `project` | Nom projet, targets, breakpoints, watch pins, settings projet |
| `command_line` | Override CLI (target passé en argument) |
| `transient` | État temporaire non persisté |

### Persistance

- **Autosave** toutes les 5 secondes
- Écriture atomique (temp + rename)
- Position/taille fenêtres, fullscreen, maximized, moniteur, DPI synchronisés en continu
- Breakpoints, watches, layout de panels : tout dans le même arbre sérialisé

### Fichiers par défaut
- `default.raddbg_user` — settings utilisateur
- `default.raddbg_project` — projet par défaut
- Migration automatique des formats pré-0.9.16

---

## 6. Intégration et interopérabilité

### Ce qui existe

| Mécanisme | Description |
|-----------|-------------|
| **CLI** | Arguments `--user:`, `--project:`, `--auto_run`, `--auto_step`, `-q`, `--jit_pid/code/addr` |
| **IPC partagé** | Shared memory + sémaphores nommés (`--ipc`) pour envoyer des commandes texte à une instance GUI |
| **Commandes texte** | Toutes les actions UI sont des commandes nommées, invocables via IPC |
| **NatVis en PDB** | Le linker radlink embarque des `.natvis` dans les PDB pour Visual Studio |
| **NatVis runtime** | Module `src/natvis/` : parsing, évaluation, expansion, cache hot-reload, intégration auto-hooks dans la frame loop |
| **`launch.vs.json`** | Config minimale pour debugger raddbg lui-même sous WSL+GDB |

### Ce qui n'existe PAS

| Élément | Statut |
|---------|--------|
| **Debug Adapter Protocol (DAP)** | Absent — aucune implémentation |
| **Extension VS Code / Cursor** | Absente — aucun `package.json`, `.vsix` |
| **Sockets TCP/UDP** | Absent — pas de serveur réseau |
| **JSON-RPC / Protocol Buffers** | Absent — IPC en texte brut |
| **Scripting (Lua, Python)** | Absent — pas de runtime embarqué |
| **Remote debugging** | Planifié, non implémenté |
| **Plugin/extension API** | Absent — extensibilité uniquement par recompilation |
| **LSP** | Absent |

### Mécanisme IPC détaillé

L'IPC fonctionne par **shared memory nommé** :

1. L'instance GUI crée des objets nommés au démarrage :
   - `_raddbg_ipc_sender2main_shared_memory_<pid>_`
   - Sémaphores signal + lock (sender→main et main→sender)
2. Un processus externe lance `raddbg --ipc <commande>` :
   - Scanne les processus pour trouver une instance raddbg
   - Ouvre le shared memory par PID
   - Écrit la commande dans le ring buffer
   - Signal via sémaphore, attend réponse
3. Les commandes sont les **mêmes noms** que l'UI interne (`RD_CmdKind_*`)

---

## 7. Points forts

### Architecture
- **Layering exemplaire** : 52 couches à dépendances acycliques, chacune isolable
- **Unity build** : compilation rapide, pas de complexité de build system
- **Zero dépendances externes lourdes** : tout est embarqué (`third_party/`)
- **Format RDI** : abstraction propre au-dessus de PDB/DWARF, performances supérieures
- **Code C propre** : conventions de nommage cohérentes, namespaces par préfixe

### Debugger
- **Performance** : caches asynchrones partout (debug info, fichiers, artéfacts, fonts)
- **Évaluateur d'expressions** : compilateur complet (lexer→parser→typechecker→IR→eval)
- **Système de visualisation** : lenses extensibles, type views pour STL/UE, **support NatVis runtime**
- **NatVis** : parsing XML, modèle typé, traduction d'expressions, expansion (ArrayItems, LinkedListItems, CustomListItems, Synthetic, ExpandedItem), cache hot-reload
- **Mémoire** : inspecteur mémoire avec peek types, annotations struct, zoom dédié
- **Linker intégré** : radlink 50% plus rapide sur gros projets, support natif RDI

### UI
- **Thèmes** : système de patterns par tags, personnalisable
- **Layout flexible** : splits récursifs, onglets, drag & drop
- **Commandes nommées** : toute action est une commande, facilitant l'automatisation
- **Autosave** : état complet persisté automatiquement

---

## 8. Limitations et axes d'amélioration

### Ergonomie

| Problème | Impact | Suggestion |
|----------|--------|------------|
| Pas de **command palette** accessible type Ctrl+Shift+P | Discoverability réduite | Exposer la palette de commandes de façon plus proéminente |
| **Raccourcis non documentés** dans l'UI | Courbe d'apprentissage | Tooltips avec raccourcis, cheat sheet intégrée |
| Format de config **propriétaire** | Difficulté d'édition manuelle | Envisager un format standard (TOML/JSON) ou fournir un éditeur dédié |
| **Pas de remote debugging** | Limite les use cases (containers, consoles, CI) | Priorité roadmap — architecture `demon` prête pour ça |

### Intégration

| Problème | Impact | Suggestion |
|----------|--------|------------|
| **Pas de DAP** | Impossible d'intégrer nativement dans VS Code/Cursor | Implémenter un adaptateur DAP (voir section 9) |
| **IPC par shared memory uniquement** | Limité au même machine, API non documentée publiquement | Ajouter un transport TCP/pipe nommé + protocole documenté |
| **Pas d'API de plugin** | Pas d'écosystème d'extensions | Définir une API stable (au moins pour les visualiseurs) |
| **Pas de scripting** | Automatisation limitée aux commandes texte | Envisager un runtime minimal (Lua ?) pour macros/automatisation |
| **Pas de LSP/symbiose éditeur** | Le debugger et l'éditeur sont deux mondes séparés | Explorer l'intégration bidirectionnelle avec les IDE |

### Technique

| Problème | Impact | Suggestion |
|----------|--------|------------|
| **Windows x64 uniquement** (production) | Pas de Linux, pas de ARM | Port Linux en cours — prioriser |
| **PDB uniquement** (production) | Pas de DWARF complet | Convertisseur DWARF→RDI en cours |
| **Pas de tests unitaires exposés** | Difficile de contribuer avec confiance | Le torture test existe mais n'est pas structuré comme une suite de tests |
| **Pas de CONTRIBUTING.md** | Barrière à la contribution | Ajouter un guide de contribution |

---

## 9. Pistes d'intégration avec Cursor

### 9.1. Adaptateur DAP (Debug Adapter Protocol)

**Le chemin le plus impactant** pour intégrer RAD Debugger dans Cursor/VS Code.

#### Approche recommandée

```
┌──────────┐     DAP (JSON-RPC/TCP)     ┌────────────────┐     IPC shared mem     ┌──────────┐
│  Cursor  │ ◄──────────────────────────► │  raddbg-dap    │ ◄───────────────────── │  raddbg  │
│  (IDE)   │                              │  (adaptateur)  │      ou commandes      │  (GUI)   │
└──────────┘                              └────────────────┘                        └──────────┘
```

**Option A — Adaptateur externe (recommandé à court terme)**
- Écrire un programme `raddbg-dap` qui :
  - Écoute en TCP (DAP standard) côté IDE
  - Communique avec `raddbg` via l'IPC shared memory existant
  - Traduit les messages DAP ↔ commandes raddbg
- Avantage : ne modifie pas le code source de raddbg
- Limite : l'IPC shared memory n'expose pas tous les événements nécessaires (ex: stopped, breakpoint hit, output)

**Option B — DAP natif intégré (recommandé à moyen terme)**
- Ajouter un mode `--dap` à raddbg qui :
  - Écoute sur stdin/stdout ou TCP
  - Implémente le protocole DAP directement
  - Accède nativement à toutes les couches `ctrl`/`dbg_engine`/`eval`
- Avantage : accès complet à toutes les capacités du debugger
- Effort : significatif mais architecture bien adaptée (couches proprement séparées)

#### Fonctionnalités DAP prioritaires

1. `launch` / `attach` — lancement et attachement de processus
2. `setBreakpoints` / `setFunctionBreakpoints` — breakpoints
3. `continue` / `next` / `stepIn` / `stepOut` — stepping
4. `stackTrace` — call stack
5. `scopes` / `variables` — locals/watches
6. `evaluate` — évaluation d'expressions (l'évaluateur raddbg est très riche)
7. `threads` — liste des threads
8. `modules` — modules chargés
9. `disassemble` — désassemblage (natif dans raddbg)
10. `readMemory` — lecture mémoire

### 9.2. Extension Cursor / VS Code

Si un adaptateur DAP est implémenté, l'extension est assez simple :

```json
{
  "name": "raddbg",
  "contributes": {
    "debuggers": [{
      "type": "raddbg",
      "label": "RAD Debugger",
      "program": "./raddbg-dap",
      "runtime": "",
      "configurationAttributes": {
        "launch": {
          "properties": {
            "program": { "type": "string" },
            "args": { "type": "array" },
            "cwd": { "type": "string" }
          }
        },
        "attach": {
          "properties": {
            "pid": { "type": "number" }
          }
        }
      }
    }]
  }
}
```

### 9.3. Amélioration de l'IPC existant

Même sans DAP, l'IPC actuel pourrait être amélioré pour Cursor :

1. **Documenter les commandes IPC** — lister toutes les `RD_CmdKind_*` avec `ListInIPCDocs`
2. **Ajouter un transport pipe nommé / TCP** — plus standard que shared memory
3. **Ajouter des événements sortants** — le debugger notifie l'IDE quand :
   - Un breakpoint est touché
   - Le process s'arrête/reprend
   - La position source change
   - Un output est produit
4. **Format JSON pour les messages** — plus facile à parser que le texte brut

### 9.4. Intégration "goto source" bidirectionnelle

Même sans DAP complet, une intégration minimale utile :

**Cursor → raddbg :**
```powershell
# Ouvrir un fichier source dans raddbg à une ligne précise
raddbg --ipc "open_file path/to/file.cpp:42"
# Poser un breakpoint
raddbg --ipc "toggle_breakpoint path/to/file.cpp:42"
# Lancer le debug
raddbg --ipc "run"
```

**raddbg → Cursor :**
- Actuellement non supporté (pas d'événements sortants)
- Nécessiterait un callback/webhook quand la position source change

### 9.5. Partage de type views / visualiseurs

Le système de `type_view` de raddbg est puissant mais isolé. Une piste :
- Exporter/importer les type views en format standard
- Permettre à Cursor de contribuer des visualiseurs via config
- Mapper les NatVis (que beaucoup de projets UE utilisent déjà) vers les type views raddbg

---

## Annexe : Commandes IPC documentées

Les commandes disponibles via `--ipc` sont toutes celles marquées `ListInIPCDocs` dans `raddbg.mdesk` / `generated/raddbg.meta.c`. Parmi les plus utiles pour l'intégration :

- Navigation : `open_file`, `goto_line`, `center_cursor`
- Debug : `run`, `kill`, `step_into`, `step_over`, `step_out`, `continue`
- Breakpoints : `toggle_breakpoint`, `remove_breakpoint`
- Targets : `add_target`, `remove_target`
- Attach : `attach_to_process`
- Expressions : `watch_expression`
- Fichiers : `switch_to_partner_file`

---

_Ce document est un point de départ pour les questions sur l'ergonomie et l'intégration avec Cursor. N'hésitez pas à approfondir n'importe quelle section._
