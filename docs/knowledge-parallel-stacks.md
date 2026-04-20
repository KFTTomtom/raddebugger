# RAD Debugger - Knowledge Base: Parallel Stacks & UI System

## 1. UI Event System & Click Propagation

### `ui_signal_from_box` consomme les événements

`ui_signal_from_box(box)` itère la queue d'événements globale. Quand un événement est traité par un box (press, release, scroll), il est **mangé** via `ui_eat_event(evt)` (`taken = 1` → removal de l'event node de la liste).

**Conséquence critique** : si un parent `Clickable` appelle `ui_signal_from_box` **avant** que ses enfants soient construits et signalés, le parent consomme les événements mouse et les enfants ne les voient jamais.

**Pattern correct pour des enfants cliquables dans un conteneur pannable** :
1. Construire le conteneur **sans** `UI_BoxFlag_Clickable`
2. Construire les enfants avec `Clickable`, appeler `ui_signal_from_box` sur chacun
3. Ajouter un "pan overlay" box **après** tous les enfants, avec `Clickable`, dont le signal est traité en dernier
4. Le scroll/zoom peut rester sur le conteneur via `UI_BoxFlag_Scroll` (indépendant de `Clickable`)

```c
// Conteneur : pas de Clickable, juste Clip + Scroll + DrawBackground
UI_Box *canvas = ui_build_box_from_stringf(UI_BoxFlag_Clip|UI_BoxFlag_Scroll|UI_BoxFlag_DrawBackground, "canvas");

// ... construire les enfants cliquables dans UI_Parent(canvas) ...
// ... appeler ui_signal_from_box sur chaque enfant ...

// Scroll traité après les enfants
UI_Signal canvas_sig = ui_signal_from_box(canvas);
// canvas_sig.scroll fonctionne car Scroll est indépendant de Clickable

// Pan overlay : dernier enfant, reçoit les clics non-consommés
UI_Parent(canvas) {
    UI_Box *pan = ui_build_box_from_stringf(UI_BoxFlag_Floating|UI_BoxFlag_Clickable, "pan_overlay");
    UI_Signal pan_sig = ui_signal_from_box(pan);
    if(ui_dragging(pan_sig)) { /* panning */ }
}
```

### `hot_box_key` et priorité de hover

Le système UI utilise `hot_box_key` comme singleton. Quand un box A set `hot_box_key` à sa clé, les box suivants ne peuvent devenir hot que si `hot_box_key == zero || hot_box_key == their_key` (ligne ~3127 de `ui_core.c`). Cela signifie que le **premier** box à devenir hot bloque les suivants, sauf s'il redevient zero.

## 2. `DrawHotEffects` - Effets de survol

Le flag `UI_BoxFlag_DrawHotEffects` dessine :
- Un **drop shadow** (`dr_rect` avec blur) dont l'alpha dépend de `hot_t * box_background_color.w`
- Un **brighten constant** (`color.w *= 0.05f`) appliqué même sans hover
- Un **cercle lumineux** autour de la souris (`hot_t > 0.01f`)

**Piège** : sur des box sans fond explicite, le brighten constant peut produire un fond blanc/gris visible. Ne pas utiliser `DrawHotEffects` sur des box internes de layout (frames dans une liste). Réserver aux boutons/onglets avec un fond explicite.

## 3. Artifact Cache (`ac_artifact_from_key`)

### Fonctionnement
- Clé = `(create_fn_ptr, string_key)` → lookup dans une hashtable
- `gen` = génération attendue. Si `node->last_completed_gen != gen`, l'artifact est **stale**
- Un artifact stale est quand même retourné immédiatement (pas de blocage)
- Une requête de rebuild async est enqueued (`working_count` CAS)
- L'appelant peut passer `.stale_out = &b32` pour savoir si le résultat est stale

### `ctrl_call_stack_tree` utilise `.gen = ctrl_reg_gen()`
```c
AC_Artifact artifact = ac_artifact_from_key(access, str8_zero(),
    ctrl_call_stack_tree_artifact_create,
    ctrl_call_stack_tree_artifact_destroy,
    endt_us,
    .gen = ctrl_reg_gen());
```

Le tree est partagé globalement (clé = `str8_zero()`). Il est invalidé quand les registres changent.

### Problème de rafraîchissement des vues custom
L'artifact cache est async. Quand une vue demande le tree juste après un break, elle reçoit le résultat stale. Le rebuild se fait en background. Mais **aucun mécanisme natif ne re-déclenche le repaint** de la vue quand le rebuild est terminé.

**Solution robuste** : ne pas se baser sur `ctrl_reg_gen()` (qui change côté contrôleur, pas côté UI). Utiliser la transition `d_ctrl_targets_running()` : running → stopped = break event.

```c
B32 is_running = d_ctrl_targets_running();
U64 was_running = rd_view_setting_value_from_name(str8_lit("was_running")).u64;
U64 refresh_frames = rd_view_setting_value_from_name(str8_lit("refresh_frames")).u64;

if(was_running && !is_running) {
    refresh_frames = 20; // 20 frames de polling
}
if(refresh_frames > 0) {
    rd_request_frame(); // force le repaint
    refresh_frames -= 1;
}
```

## 4. `rd_request_frame()`

```c
internal void rd_request_frame(void) {
    rd_state->num_frames_requested = 4;
}
```

Demande 4 frames de repaint. Doit être appelé à chaque frame tant qu'on veut continuer à rafraîchir (le compteur est reset à 4 à chaque appel, pas incrémenté).

## 5. View Params (persistance par vue)

Les vues peuvent stocker des valeurs persistantes via :
```c
rd_store_view_param_f32(str8_lit("key"), value);    // F32
rd_store_view_param_u64(str8_lit("key"), value);    // U64 (format hex)
E_Value v = rd_view_setting_value_from_name(str8_lit("key")); // lecture (.f32, .u64)
```

Stocké dans le CFG du panel. Survit aux repaints mais pas au redémarrage de RAD (sauf si sérialisé dans le layout).

## 6. Settings utilisateur (raddbg.mdesk)

Ajouter un setting dans la section `@table ... RD_SettingTable` :
```mdesk
@default(0) @display_name('Mon Setting') @description("Description.")
  'mon_setting': bool,
```

Lecture dans le code :
```c
B32 val = rd_setting_b32_from_name(str8_lit("mon_setting"));
```

Apparaît automatiquement dans Settings UI.

## 7. Underflow U64 dans les boucles de reversal

**Bug classique** : `for(U64 lo=0, hi=count-1; lo<hi; ...)` quand `count == 0` → `hi` underflow à `~0`.

**Fix** : toujours garder `if(count > 1)` avant la boucle.

## 8. String8List

`String8List` utilise `.node_count`, **pas** `.count`. Erreur de compilation sinon.

## 9. Coloration syntaxique dans les vues custom

```c
// rd_code_label : crée un box DrawText avec coloration syntaxique
// Paramètres : alpha, indirection_size_change, base_color, string
rd_code_label(1.f, 0, code_color, frame_name);
```

Utilise `rd_fstrs_from_code_string` en interne pour parser et coloriser.

## 10. Flèche d'exécution (IP indicator)

Pattern pour dessiner la flèche du thread actif :
```c
CTRL_Entity *thread = ctrl_entity_from_handle(&d_state->ctrl_entity_store->ctx, thread_handle);
Vec4F32 arrow_color = rd_color_from_ctrl_entity(thread);

RD_Font(RD_FontSlot_Icons) UI_FontSize(size)
  UI_TextColor(arrow_color)
  UI_Flags(UI_BoxFlag_DisableTextTrunc)
{
    ui_build_box_from_stringf(UI_BoxFlag_Floating|UI_BoxFlag_DrawText,
        "%S###arrow_id", rd_icon_kind_text_table[RD_IconKind_RightArrow], id);
}
```

## 11. Navigation vers le source depuis un vaddr

```c
CTRL_Entity *proc = ctrl_entity_from_handle(&d_state->ctrl_entity_store->ctx, process_handle);
CTRL_Entity *module = ctrl_module_from_process_vaddr(proc, vaddr);
U64 voff = ctrl_voff_from_vaddr(module, vaddr);
DI_Key dbgi_key = ctrl_dbgi_key_from_module(module);
D_LineList lines = d_lines_from_dbgi_key_voff(arena, dbgi_key, voff);
if(lines.first != 0) {
    rd_cmd(RD_CmdKind_FindCodeLocation,
           .process   = proc->handle,
           .vaddr     = vaddr,
           .file_path = lines.first->v.file_path,
           .cursor    = lines.first->v.pt);
}
```

## 12. CTRL_CallStackTree structure

- `tree->root` : nœud racine (sentinel `ctrl_call_stack_tree_node_nil`)
- Chaque `CTRL_CallStackTreeNode` a : `vaddr`, `process`, `depth` (inline_depth), `child_count`, `first`/`next`, `threads` (HandleList)
- `depth > 0` = frame inlined
- Résolution de nom : `rdi_procedure_from_voff(rdi, voff)` → `procedure->name_string_idx`
- Si pas de symbole, le nom est formaté comme `0x%I64x` (adresse brute)

## 13. Layout d'arbre non-overlapping (2 passes)

1. **Bottom-up** : calculer `subtree_w[i]` = max(branch_w, somme des subtree_w des enfants + gaps)
2. **Top-down** : assigner X en centrant les enfants dans l'espace `subtree_w` du parent

```c
// Pass 1: bottom-up
for(U64 i = count; i > 0; i -= 1) {
    U64 idx = i - 1;
    F32 children_total = /* somme subtree_w des enfants */ ;
    subtree_w[idx] = Max(branch_w, children_total);
}
// Pass 2: top-down (roots first, then children)
for(U64 i = 0; i < count; i += 1) {
    // centrer les enfants de branches[i] dans subtree_w[i]
}
```

## 14. Build RAD Debugger

```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"" >nul 2>&1 && cd /d D:\Tools\raddebugger && build.bat raddbg"
```

- Nécessite l'environnement MSVC (vcvars64.bat)
- `telemetry` est un flag **opt-in** (l'ajouter active le telemetry, ne pas l'ajouter le désactive)
- `LINK : fatal error LNK1104: cannot open file 'raddbg.exe'` = l'exe est encore en cours d'exécution
- Le metagen parse les fichiers `.mdesk` et génère `generated/raddbg.meta.h`

## 15. Perforce workflow

Toujours utiliser `p4 edit` avant de modifier un fichier read-only. Ne jamais utiliser `attrib -R`.
