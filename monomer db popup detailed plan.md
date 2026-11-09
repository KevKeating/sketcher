# Monomer Database popup: implementation plan (SKETCH-2827)

This is the single source of truth for SKETCH-2827. The work is split into six
steps, one PR per step. Each step is one self-contained brief for a sub-agent,
and each ends in a building, tested, reviewable state. A sub-agent should read
**Goal and background**, **Design decisions** and **Shared context**, then
only its own step.

---

## Goal and background

We're adding a side-bar button that opens a popup for searching the monomer
database. Choosing a monomer in the popup updates `SketcherModel` so that the
scene tool becomes a `DrawMonomerSceneTool` for that monomer.

- **Jira:** SKETCH-2827.
- **Spec:** `F:\Documents\sketcher\SKETCH-2827\Dual Mode Sketcher v1 - design
  updates.docx`, section "Using the Monomer DB" → "Longer term approach".
  - Main popup mockup: `image-20260727-212310.png`.
  - Filter popup mockup: `image-20260728-133216.png`.
  - Both are in the same folder.
  - The docx also has a "No monomer database found" empty-state mockup.
    This plan doesn't implement it (see "Deviations from the spec").
- **Offline discussion has changed parts of the spec.** Where this plan and
  the spec differ, the plan wins. Every known difference is listed under
  "Deviations from the spec".
- **Out of scope:**
  - the "Matching SMILES" filter row;
  - the "Recently used items" palette (press-and-hold on the last-used
    button).
- **Qt independence:** we may move away from Qt in the near future. All
  filtering, sorting and description logic must therefore be Qt-free. The Qt
  table model is only a thin adapter over it.
- **The new UI replaces the "Unclassified" buttons** added in `5a9c9405`. It
  also lets us simplify the long-press monomer-button popups so that they show
  only core monomer IDs, with displayed metadata resolved custom-first.

---

## Design decisions

### The table

- **The table contains custom monomers plus the core monomers that have no
  natural analog.**
  - "No natural analog" is the type-specific "None" rule below: empty, or
    `X` for peptides and `N` for nucleic acids.
  - In today's core DB this adds only `ac` and `am` (PEPTIDE Terminal, analog
    `X`). Their only entry point today is the Unclassified popup, which this
    work removes.
  - Every other core monomer is reachable through the side-bar buttons and
    their long-press popups, so it's excluded from the table.
  - A custom monomer with the same symbol and polymer type as an included
    core one replaces it, matching the custom-first rule of
    `get_info_from_database`. The core row isn't shown as well.
- **One read per database change; everything else happens in memory.**
  - `MonomerDatabase` gets two small public calls, `getCustomMonomers()` and
    `getCoreMonomers()`. Both use one shared row-reading loop.
  - A pure, Qt-free function builds the table's source list from them.
  - Filtering, text search and sorting are pure, Qt-free functions over
    that cached list.
- **Use a `QAbstractTableModel` subclass** for the table model, not a bare
  `QAbstractItemModel`.
- **Live filtering; no Apply button.** The back-of-the-envelope cost for the
  12k-monomer ChEMBL file (`chembl_monomers_x5.json`) is:
  - filter by type and analog (a few enum or short-string compares per row):
    about 1 ms native;
  - case-insensitive substring search on symbol and name: about 2–3 ms;
  - sort (about 170k case-insensitive comparisons): about 10–20 ms;
  - resetting a `QTableView` with fixed row heights (it lays out only the
    visible rows): a few ms.

  WASM is roughly 2–3× slower than native, so the worst case is well under
  100 ms, comfortably inside the 400 ms budget. Three conditions keep it that
  way:
  - never re-read the DB on a filter change;
  - never use `ResizeToContents` on the vertical header;
  - pass row indices around rather than copying `MonomerInfo`, which holds
    ten optional strings.

  If testing shows otherwise, adding an Apply button back is a small change.

### Buttons and tool state

- **One shared monomer-database button pair.** The last-used button and the
  DB (folder) button sit outside both side-bar pages:
  - below the horizontal `line`;
  - above `custom_monomer_btn`.

  Neither button is in `amino_monomer_group` or `nucleic_monomer_group`.
  **Don't copy the `custom_monomer_btn` / `DrawTool::CUSTOM_MONOMER` code as a
  pattern.** Much of it was meant to be temporary.
- **Each page remembers its own DB monomer, stored in `SketcherModel`.**
  - Replace `ModelKey::MONOMER_DB_MONOMER` with two keys:
    - `AMINO_ACID_DB_MONOMER`: a PEPTIDE or CHEM `MonomerID`;
    - `NUCLEIC_ACID_DB_MONOMER`: an RNA or CHEM `MonomerID`.
  - Add `SketcherModel::getMonomerDbMonomer()`, which returns the key for the
    current `MONOMER_TOOL_TYPE`. The scene, the mutation code, the side bar
    and the popup all go through it.
  - These keys are the last-used slots. There's no separate widget member.
    The shared last-used button shows the current page's value, so it
    changes when the page changes.
- **No per-page "DB tool" marker.** `AminoAcidTool::UNCLASSIFIED` and
  `NucleicAcidTool::UNCLASSIFIED` are removed. A page's `AMINO_ACID_TOOL` /
  `NUCLEIC_ACID_TOOL` always names its last standard monomer button, and
  whether the page's last tool was its DB monomer is part of the per-page
  record below.
- **Each page restores its last monomeric draw tool.** Switching to the other
  page, or to the atomistic tools, and straight back must leave the tool
  selection exactly as it was.
  - `SketcherModel` records the last monomeric draw tool used on each page
    (`MONOMER`, `MONOMER_DB_MONOMER`, `CUSTOM_MONOMER` or
    `MONOMERIC_CONNECTION`) whenever `DRAW_TOOL` or `MONOMER_TOOL_TYPE`
    changes, so the code that activates tools doesn't have to.
  - `SketcherModel::getDrawToolToRestore(page)` returns that tool, falling
    back to `MONOMER` when it can no longer be used: a DB tool with an empty
    slot, or a custom monomer that the other page replaced. Both pages share
    one stored custom monomer, so the AA page can restore a custom peptide
    or CHEM monomer, and the NA page a custom nucleic acid or CHEM monomer.
  - Page switching (`MonomerToolWidget::onAminoOrNucleicBtnClicked`) and the
    atomistic→monomeric toggle (`SketcherSideBar::updateCheckState`) both use
    it.
  - Both only replace a tool from the other set. A shared tool (select,
    move-rotate or erase, i.e. anything `is_monomeric_draw_tool()` rejects)
    stays active across a page switch, just as it already did across the
    atomistic↔monomeric toggle, so a page switch never switches to or away
    from one. No extra state is needed: the shared tool simply remains
    active, and the per-page record is untouched.
- **Choosing a monomer switches the side-bar page to match it.**
  - A PEPTIDE monomer switches to AMINO_ACID, and an RNA monomer to
    NUCLEIC_ACID.
  - A CHEM monomer leaves the page as it is, and is stored as the current
    page's DB monomer.
  - Without a canvas selection, choosing sets these together:
    - `DRAW_TOOL = MONOMER_DB_MONOMER`;
    - `MONOMER_TOOL_TYPE`;
    - that page's `*_DB_MONOMER`.

    The page's `AMINO_ACID_TOOL` / `NUCLEIC_ACID_TOOL` is left alone.
  - With a canvas selection, choosing does three things:
    - it mutates the selection by setting that page's `*_DB_MONOMER`;
    - it switches the page;
    - it leaves the draw tool unchanged.

    Mutation updates the page's last-used DB slot, but deliberately leaves
    the draw tool, and so the page's last monomeric tool, unchanged. This is
    a product behavior, not a workaround for duplicate mutation:
    `AMINO_ACID_TOOL` no longer mutates the selection; `AMINO_ACID_SYMBOL`
    does. `setValues()` pings every key it is given, so check every key
    against `SketcherWidget::applyModelValuePingToTargets` before including it.

    `MolModel::mutateMonomers` skips selected monomers whose type doesn't
    match the chosen monomer. So committing an RNA row with only peptides
    selected switches to the NA page and updates its last-used slot, but
    changes nothing on the canvas and adds no undo entry. That's accepted
    behavior; don't add special handling for it.
- **Page synchronization is independent of the draw tool.** Synchronize the
  AA/NA page buttons and `amino_or_nucleic_stack` with `MONOMER_TOOL_TYPE`
  even while Select or Move/Rotate is active. Choosing a mutation target can
  switch the visible page without changing the draw tool.
- **`SketcherModel` repairs its own DB state when the database changes.**
  The repair touches only `SketcherModel` keys, so it lives there, not in
  `MonomerToolWidget`.
  - `SketcherModel` subscribes to `MonomerDatabase::subscribeToChanges()` in
    its constructor. It keeps the `scoped_connection` as a member and
    marshals the callback with `QMetaObject::invokeMethod(this, …)`.
  - A page's `*_DB_MONOMER` is valid when it's in the table's source list.
    Check this with Step 1's `get_monomer_database_table_monomers()`; don't
    re-implement the inclusion rule. This is about membership, not
    drawability. For example, if a custom override of core `meG` is
    unloaded, `meG` reverts to a core monomer with analog G, leaves the
    table, and is cleared from the slot, even though it can still be drawn
    from the G palette.
  - For each page whose slot is no longer valid, including an inactive page
    and while atomistic mode is active:
    - reset the slot to an empty `MonomerID`, which blanks the last-used
      button for that page;
    - if that page's DB tool is the active draw tool, set `DRAW_TOOL` to
      `MONOMER`, which activates the page's last standard monomer button;
      otherwise keep the active draw tool. The page's `AMINO_ACID_TOOL` /
      `NUCLEIC_ACID_TOOL` and symbol are never changed;
    - a page whose last monomeric tool was its DB tool then restores its
      standard tool, because `getDrawToolToRestore()` requires a non-empty
      slot.
  - Apply all the repairs in one update that emits `valuesChanged` (so the
    scene and widgets refresh) but never `valuePinged` (so nothing mutates
    a canvas selection or adds an undo entry). Do this with a **private**
    helper: refactor `setValues()` so that it and the new helper share the
    store-and-emit code. Don't add a ping parameter or a new public setter.
  - The logic is small, so it belongs in `SketcherModel` itself. Move it to a
    separate friend class only if it grows well beyond that.
- **Database-change listeners are independent of each other.** Several
  objects subscribe to database changes: `SketcherModel` (the repair), the DB
  popup (its cached table rows, from Step 3) and `MonomerToolWidget` (the
  long-press palettes). They run in subscription order, which depends on
  construction order, so none may depend on another having run first:
  - each listener reads the database directly, never another listener's
    cache;
  - the popup's selection sync tolerates a model ID that isn't in its cache
    yet (it selects nothing), and the popup re-syncs to the model after each
    of its own refreshes.
- **Table selection mirrors the model, and only for the DB tool.** A row is
  selected only when:
  - `getDrawTool() == DrawTool::MONOMER_DB_MONOMER`,
  - there's no canvas selection, and
  - `getMonomerDbMonomer()` is one of the current rows.

  Otherwise no row is selected. No row is selected by default; this overrides
  the spec's "first item should probably be selected". The canvas-selection
  condition matches the last-used button's checked rule. It's mostly
  defensive: `SketcherModel::onSelectionChanged()` already switches a drawing
  tool to Select whenever a canvas selection appears.
- **The popup has two modes, fixed while it's open.** The popup reads
  `SketcherModel::hasActiveSelection()` once, when it opens:
  - **Tool mode** (no canvas selection): changing rows with the mouse or
    keyboard activates the row's monomer live, as before.
  - **Mutation mode** (canvas selection): keyboard navigation only moves the
    table's current index. It doesn't mutate, update a last-used slot or
    switch pages. A click on a valid row, Enter/Return or Space commits that
    row's mutation. The table selection stays empty, but the current index
    is kept for keyboard navigation and preview, including after a commit.
    Stop the selection at its source: a small `QTableView` subclass
    overrides `selectionCommand()` to return `QItemSelectionModel::NoUpdate`
    in this mode, rather than clearing a selection after Qt has made it.

  **Why the mode can be fixed.** Nothing the user does inside the popup
  changes the canvas selection:
  - A commit mutates monomers in place (`MolModel::mutateMonomers`), so the
    selection snapshot is unchanged.
  - While a `Qt::Popup` is active, Qt matches window-context shortcuts
    against the popup rather than the main window, so the Sketcher's
    Ctrl+A, Ctrl+D, Ctrl+I and Ctrl+Z actions (`MoreActionsMenu`, default
    `Qt::WindowShortcut`) don't fire. Ctrl+A selects the search text in the
    search box and does nothing in a single-selection table. Verify this with
    tests rather than relying on it.

  The selection can still change programmatically while the popup is open,
  for example through a host API call such as the WASM JS API or Maestro's
  Python API. The popup listens to `SketcherModel::selectionChanged` and
  **closes itself** when `hasActiveSelection()` no longer matches the value
  it read on opening. The user can reopen it in the new mode. Don't close it
  on every `selectionChanged`, since the scene may re-emit that signal while
  rebuilding items after a commit.
- **Enter/Return and Space apply the current row in both modes and never
  close the popup.** In mutation mode they commit it. In tool mode they
  activate it, which does nothing if it's already the active monomer (it
  usually is, because changing rows activates). Handle both keys in one
  place, only when the table has focus; consume them and ignore auto-repeat.
- **The current row has its own subtle highlight.** Without a selected row,
  only Qt's focus rectangle would mark the current row, and it can be faint
  or invisible, especially on WASM. Paint the current row's background in a
  light color whenever the current row isn't the selected row. Use
  `#edf7fc`, the side bar's existing hover color, which is a pale tint of the
  `#d4e6f1` checked color, and use `#d4e6f1` for the selected row. Make sure
  the whole old and new rows repaint when the current index changes.
- **Preview row:** use the selected row if there is one, otherwise the valid
  current index. Selection and current index are independent; clearing the
  selection must not clear the current index. If neither exists, the preview
  is empty. Programmatic synchronization never activates or mutates a monomer.
- **Opening the popup never changes the active tool.** Only these do:
  - choosing a row;
  - clicking a non-blank last-used button.

  A blank last-used button does nothing and looks like empty space.

### Popup and filter behaviour

- **Clicking outside the popup dismisses it. On native builds, the click
  also reaches what's underneath** (e.g. the canvas). **On WASM, it doesn't:**
  the click only dismisses the popup. This matches the existing long-press
  monomer popups, which behave the same way on `main` (confirmed manually on
  Windows and in a WASM build; not caused by this work or by simulated
  clicks). Treat it as a known platform difference: don't try to make WASM
  replay the click in this work. Don't suppress replay globally. Reuse
  `configure_button_popup(button, popup)` from `widget/widget_utils.h` for
  both the DB button and the filter button: it sets `WA_NoMouseReplay` only
  for a dismissing press over the originating button, preventing immediate
  reopening while preserving replay to the canvas and other controls.
- **Popup state and reset rule.** One popup instance exists. It records the
  `MONOMER_TOOL_TYPE` at the moment it **closes**, however it closes. On
  opening:
  - **First opening, or the page now differs from the page at the last
    close:**
    - clear the search text;
    - reset the filter to the page default:
      - AMINO_ACID: polymer type {Peptide, Structure}, monomer type Any,
        natural analog Any or None;
      - NUCLEIC_ACID: polymer type {Nucleic acid, Structure}, Any, Any or
        None.
  - **Otherwise:** the search text, filter and sort are kept.

  Recording the page at close time is what lets the page switch when a row
  is chosen. For example:
  1. The user opens the popup on the AA page.
  2. They enable RNA in the filter and choose an RNA row, which switches to
     the NA page.
  3. They close the popup, which records NA.
  4. They reopen it on the NA page, and nothing is reset.

  The filter only resets when the user changes page while the popup is
  closed.

  **Clear All** sets the filter to its most permissive state: all three
  polymer types, monomer type Any, and natural analog Any or None, regardless
  of the current page. It leaves search text and sort unchanged. This is
  distinct from the page-default reset on opening: Clear All leaves zero
  filters applied, no bold combos, and a normal filter button.
- **Anchor.** A page switch while the popup is open must not move the popup's
  anchor. The side bar's `amino_or_nucleic_stack` sizes itself to its
  largest page, so the shared buttons below it shouldn't move. Verify this.
- **Popup bounds and resizing.**
  - **Available area:** `anchor->screen()->availableGeometry()`. That is the
    desktop on native builds and the hosting canvas on WASM, because each
    WASM canvas is its own `QScreen` (see `ModalDialog::showEvent`). Use this
    one code path; don't branch on the platform.
  - **Placement order:**
    1. below the anchor, at full size, if it fits;
    2. otherwise above the anchor, at full size, if it fits;
    3. otherwise on whichever side has more room. The DB popup shrinks to
       fit that side, down to its minimum layout (see below); other popups
       keep their size;
    4. finally, clamp the position to the available area.
  - Keep the top-left reachable. Apply the bottom/right limits first and
    the top/left limits last, matching `ModalDialog::moveOntoScreen()`. If a
    popup is larger than the available area, top/left take precedence and
    any unavoidable overflow extends to the right/bottom.
  - The DB popup owns its layout adaptation: shrink the table viewport and
    scale the structure preview to fit, preserving the image's aspect ratio
    and readable UI text. The table retains its normal scrolling. Keep the
    title, search/filter controls and close controls accessible whenever the
    minimum layout fits.
  - If the minimum layout still cannot fit, use the top-left clamping rule.
    Do not add a scrollable whole-popup body or further layout adaptations
    for exceptionally small canvases in this work; those can be added later
    if needed.
  - Recalculate size and position on opening and when the available area
    changes while the popup is open, including canvas shrinkage. Reposition
    any open nested filter popup against its anchor and the updated bounds.
    Ordinary page switches retain the stable anchor described above.
  - Shared positioning helpers own placement and clamping only; the DB
    popup owns shrinking its contents. Do not impose DB-specific resizing
    behavior on other existing popups.
  - **Existing popups change too, deliberately.** `ToolButtonWithPopup`
    will use the shared helper (Step 2), so the long-press popups gain the
    placement order and top/left clamping. They also switch from
    `QApplication::screenAt(QCursor::pos())->geometry()` to the anchor's
    available area. They're positioned only when shown; live recalculation
    while open is only for the DB popup and its filter popup.
- **Polymer type is a multi-select combo** with "Peptide", "Nucleic acid"
  and "Structure" (CHEM). Its contents don't depend on the page.
- **Applicability rules.** Structure (CHEM) never affects applicability.
  - The **Monomer type** combo applies only when Peptide is checked and
    Nucleic acid isn't: {Peptide} or {Peptide, Structure}. It only applies
    to amino acids.
  - The **Natural analog** combo applies only when exactly one of Peptide and
    Nucleic acid is checked, with or without Structure.
  - An inapplicable combo is disabled and **shows** its effective value,
    "Any" / "Any or None", so a disabled combo never displays a value that
    isn't applied.
  - Both page defaults therefore start with the Natural analog combo
    enabled. The AA default also starts with Monomer type enabled.
- **Values hidden by applicability are remembered, not cleared.** A combo
  that becomes inapplicable only because the polymer types changed keeps
  the user's choice, and restores it when it applies again. For example:
  {Peptide} + Backbone + C, then check Nucleic acid (both combos disable and
  show Any / Any or None), then uncheck it: Backbone and C come back.
  - **Monomer type:** remember its value, but forget it (back to Any) as
    soon as the polymer types make the Natural analog combo apply to
    Nucleic acid, that is, Nucleic acid checked and Peptide not. Monomer
    type only applies to peptides, so it can't be restored for the wrong
    type, but the user has then moved on to nucleic acids, and a forgotten
    Backbone or Terminal shouldn't reappear when they return. This is the
    same moment, and the same two-toggle barrier, as forgetting a peptide
    analog.
  - **Natural analog:** remember one value **together with the polymer type
    it was chosen for** (Peptide or Nucleic acid). When the combo applies
    again:
    - for the same polymer type, restore the value;
    - for the other polymer type, forget it and use Any or None.

    This applies to every analog value, including Any and None, because
    "peptides with no natural analog" and "nucleic acids with no natural
    analog" are different questions. Peptide C (Cys) and nucleic-acid C
    (cytosine) show why a value must never carry across types. Forgetting
    takes at least two polymer-type toggles (check the new type, uncheck the
    old one, in either order), which is a deliberate barrier. Remembering
    one value per type was rejected: it could restore a value the user had
    long forgotten after time spent on the other type.
  - **Explicit resets forget everything:** Clear All, the page-default reset
    on opening, and `setFilter()` (the Step 5 folder entry).
  - The memory lives in `MonomerDatabaseFilter`, not in the combo widgets.
    The filter popup is rebuilt from the filter each time it opens, and the
    analog combo's items are rebuilt when the polymer types change, so a
    widget can't hold it.
- **How CHEM rows match.** CHEM monomers have no monomer type and no natural
  analog.
  - They pass the monomer-type filter only for "Any".
  - They pass the analog filter only for "None" and "Any or None".
- **"Filtering" baseline.** One baseline drives three things: the "N
  filters applied" count, the bold combos and the filter button's look. The
  baseline is the most permissive filter: all three polymer types, Any, and
  Any or None.
  - **Count:**
    - 1 if the polymer types aren't all three;
    - +1 if an applicable monomer type isn't Any;
    - +1 if an applicable analog isn't Any or None.
  - **Bold:** a combo is bold when it contributes to the count.
  - **Filter button:** it looks pressed (orange
    `filter-menu_lb_filtering.png`) when the count is above 0, and normal
    otherwise.
  - So the page-default state reads "1 filter applied", shows Polymer type
    in bold, and has an orange button.
- **Natural analog "None"** means empty, or the polymer type's unknown
  residue: `X` for peptides, `N` for nucleic acids.
  - Current databases use `X`/`N`. SHARED-12734 will switch them to an empty
    string. Count both.
  - `N` is asparagine for peptides, so the check depends on the polymer
    type.
  - **"Any"** is the complement of None.
  - **"Any or None"** applies no analog filtering.
- **Natural-analog choices:**
  - **Peptide:** the 20 standard one-letter codes (includes `N`; excludes
    `X`, which None covers).
  - **Nucleic acid:** `R, P, A, C, G, U, T` (the existing base display
    order). `dR` is omitted, because a sugar whose analog is deoxyribose
    uses `R` in the natural-analog field.
- **Type column:**
  - The table has Symbol, Name, Type and Analog columns. Type shows
    "Peptide", "Nucleic acid" or "Structure".
  - The column is shown when the *filter* allows more than one polymer
    type, and hidden when it allows exactly one. The rows actually displayed
    don't affect it.
  - None analogs (`X`/`N`/empty) display blank.
- **Description text** (Step 6) describes the filter. Its form is
  `<polymer types>[, <monomer type>] monomers[ with <analog>]`, or
  `<polymer types>[, <monomer type>], <X> analogs` for a specific analog.
  - **Drop "structure"** when the other filters exclude CHEM: when the
    monomer type isn't Any, or the analog is Any or a specific analog.
    "Peptide or structure" + Terminal therefore reads `Peptide, terminal
    monomers`.

  | Filter | Description |
  |---|---|
  | {Peptide} | `Peptide monomers` |
  | {Nucleic acid} | `Nucleic acid monomers` |
  | {Structure} | `Structure monomers` |
  | {Peptide, Structure} | `Peptide or structure monomers` |
  | {Nucleic acid, Structure} | `Nucleic acid or structure monomers` |
  | {Peptide, Nucleic acid} | `Peptide or nucleic acid monomers` |
  | All three | `All monomers` |
  | Peptide + Terminal | `Peptide, terminal monomers` |
  | Peptide + analog C | `Peptide, Cys analogs` (peptides use three-letter codes) |
  | Peptide + Backbone + analog C | `Peptide, backbone, Cys analogs` |
  | Nucleic acid + analog A | `Nucleic acid, A analogs` |
  | {Peptide, Structure} + Terminal | `Peptide, terminal monomers` |
  | {Peptide, Structure} + analog C | `Peptide, Cys analogs` |
  | Peptide + analog None | `Peptide monomers with no natural analog` |
  | {Peptide, Structure} + analog None | `Peptide or structure monomers with no natural analog` |
  | Peptide + Terminal + analog None | `Peptide, terminal monomers with no natural analog` |
  | Peptide + analog Any | `Peptide monomers with any natural analog` |

- **Load Database… / Update Database… links:**
  - "Load Database…" is shown when no custom monomers are loaded, and
    "Update Database…" otherwise. Both sit at bottom-left, as in the
    mockups.
  - They're shown by default.
  - `SketcherWidget::setLoadMonomerDatabaseVisible(false)` hides them, along
    with the import menu's "Load Monomer Database…". The WASM JS API
    `sketcher_load_custom_monomers` (`src/app/main.cpp`) calls that
    automatically after a host page loads monomers.
  - Both links and the import-menu action run the same extracted function.
  - **Load/update lifecycle:** before a popup link launches the file chooser,
    hide any nested filter/dropdown popups and the DB popup. Leave the DB
    popup closed after success, cancellation, partial import, or failure;
    never reopen it automatically. Hiding records the page at close as usual.
  - Preserve search text, filter and sort while hidden. Database-change
    notifications still refresh cached data, remembered IDs and preview
    validity. On normal reopening, apply the existing page-at-close reset
    rule; reopening on the same page preserves those settings.
  - Parent the chooser and load-result dialogs to a stable owning Sketcher
    widget/window rather than the hidden popup. Share the existing load/error
    handling with the import-menu action; popup dismissal belongs to the
    popup-link handler, not to a duplicate loader implementation.
- **No "No monomer database found" state.** The table's source list always
  has at least one monomer, because the core DB provides `ac` and `am`, so
  the popup always shows its normal content. Don't add a `QStackedWidget`
  or an empty page.
- **"No monomers match search & filter criteria"** (gray italics) is shown
  when the search and filters leave zero rows. The spec shows it inside the
  empty table with the header still visible, so paint it in the table's
  viewport when there are zero rows (for example, a small `QTableView`
  subclass).
- **The structure image (Step 6) renders the monomer's DB SMILES as-is.**
  Capping groups and atom-map numbers are shown as stored. Nothing is capped
  or converted.

### Shared helpers (extract; don't duplicate)

These come from the user's style rule.

- `show_popup_next_to_widget(QWidget* popup, QWidget* anchor)` (Step 2).
  This is the positioning logic in `ToolButtonWithPopup::showPopup()`, which
  is protected and button-specific. Apply the bounds rules above.
  `ToolButtonWithPopup::showPopup()` calls it afterwards. Extract the
  reusable position-clamping calculation from `ModalDialog::moveOntoScreen()`
  so dialogs and popups share it without changing the dialog's existing
  top-left precedence. Keep DB-popup layout adaptation outside this helper.
- `paint_text_with_fade_out(...)` and `elide_text()` (Step 3). These are the
  fade painting from `AbstractMonomerItem::paint()`
  (`molviewer/abstract_monomer_item.cpp`).
- `load_monomer_database_from_file(QWidget* parent)` (Step 3). This is the
  body of `SketcherTopBar::onLoadMonomerDatabaseClicked()`.
- The SMILES → RDKit mol → image path in
  `get_tool_tip_containing_smiles_image()` (Step 6).

### Separate sort from filter

**Changed from the initial plan.** The sort field and direction live in their
own small struct rather than inside `MonomerDatabaseFilter`. This keeps two
things clean:
- Clear All and the "is filtering" comparison never touch the sort;
- the sort persists across filter resets.

### Deviations from the spec

| Spec | This plan |
|---|---|
| Polymer type: single-select (PEPTIDE / RNA / CHEM / Any) | Multi-select ("Peptide", "Nucleic acid", "Structure"), the same on both pages |
| Default filter: PEPTIDE + backbone on the AA tab, RNA on the NA tab | AA: {Peptide, Structure}. NA: {Nucleic acid, Structure}. Monomer type Any, analog Any or None |
| Apply button | No Apply. Changes apply live |
| Table includes all monomers; first item selected by default | Custom monomers, plus core monomers with no natural analog. No default selection; the selection mirrors the active DB tool |
| Monomer type: chooser contents depend on the polymer type | Monomer type and Natural analog stay enabled with Structure checked, and CHEM rows only match their permissive values |
| Opening the popup loads the last-used item | It doesn't. Only the last-used button does |
| Peptide analog list includes X and omits N | Includes N and omits X (X is "None") |
| No Type column | A Type column, hidden when the filter allows one polymer type |
| Image shows a capped structure | The DB SMILES as stored, with capping groups and atom-map numbers |
| "No monomer database found" state with "Load Database…" | No empty state: the table always has core monomers with no natural analog. "Load Database…" / "Update Database…" depends on whether custom monomers are loaded |
| Matching SMILES filter; Recently-used palette | Out of scope |

---

## Shared context for every step

- **Skills and repo rules**
  - Read `AGENTS.md` / `CLAUDE.md`: build commands, formatting, staging new
    files, and no commits.
  - Use the `sketcher-architecture`, `sketcher-monomer-helm` and
    `sketcher-build-and-debug` skills.
  - All steps use `schrodinger-cpp-style`. Steps 3–6 also use
    `implement-ux-spec`.
- **Terminology in code.** This plan says "slot" for a page's last-used
  database monomer (`AMINO_ACID_DB_MONOMER` / `NUCLEIC_ACID_DB_MONOMER`).
  Don't use "slot" in code or comments; say "last-used database monomer".
  Don't say a page's "remembered tool" either. That described the removed
  `MONOMER_DB` marker. Say "last monomeric tool" (the per-page record behind
  `getDrawToolToRestore()`) or "standard tool" (`AMINO_ACID_TOOL` /
  `NUCLEIC_ACID_TOOL`).
- **Monomer DB facts**
  (`include/schrodinger/rdkit_extensions/monomer_database.h`):
  - **Singleton.** `MonomerDatabase::instance()` always holds the core DB,
    `m_core_monomers_db`.
    - It's generated from
      `src/schrodinger/rdkit_extensions/default_monomer_definitions.json`.
    - It holds 70 monomers: 60 PEPTIDE Backbone, 2 PEPTIDE Terminal with
      analog `X` (`am`, `ac`), and 8 RNA. There are no CHEM monomers.
    - It can additionally hold one custom DB, and loading a new one replaces
      it.
  - **Lookup.** Single lookups (`get_info_from_database`) prefer custom over
    core.
  - **`MonomerInfo`** holds ten `std::optional<std::string>` fields.
  - **`MONOMER_TYPE`** values seen: `Backbone`, `Branch`, `Terminal`. Tests
    use lowercase, so compare case-insensitively.
  - **`subscribeToChanges()`** returns a `scoped_connection`. Marshal the
    callback with `QMetaObject::invokeMethod(this, …, Qt::AutoConnection)`,
    as `MonomerToolWidget` does.
  - **DNA monomers use `ChainType::RNA`.** Don't add `ChainType::DNA`
    branches or tests.
    - `get_merged_na_analogs()` (`rdkit/monomer_analog.cpp`) queries
      `ChainType::DNA`. Leave it alone for this work.
- **Tool state** lives in `SketcherModel` (`model/sketcher_model.h`).
  - **Today, a DB monomer is active** when:
    - `getDrawTool() == DrawTool::MONOMER_DB_MONOMER`;
    - `ModelKey::MONOMER_DB_MONOMER` holds an `rdkit_extensions::MonomerID`;
    - the page's `AMINO_ACID_TOOL` / `NUCLEIC_ACID_TOOL` is `UNCLASSIFIED`.

    Step 2 changes this to the two per-page keys and removes the
    `UNCLASSIFIED` marker (see "Buttons and tool state").
  - **Scene tool.** `Scene::getNewSceneTool()` builds the
    `DrawMonomerSceneTool` from the `MonomerID` alone.
  - **Selection mutation.** With an active canvas selection, a `setValue` or
    ping of the DB-monomer key makes `SketcherWidget::onModelValuePinged`
    mutate the selected monomers (`sketcher_widget.cpp`, `case
    ModelKey::MONOMER_DB_MONOMER`).
  - **`setValues()`** stores *and* pings every key. Mutation is handled in
    `applyModelValuePingToTargets`, called by `onModelValuePinged`.
    `AMINO_ACID_SYMBOL` mutates a selection; `AMINO_ACID_TOOL` explicitly
    does not. DB mutation deliberately leaves the draw tool unchanged.
  - **Don't treat `custom_monomer_btn` / `DrawTool::CUSTOM_MONOMER` as a
    pattern.** Much of that code was meant to be temporary.
- **C++ tests**
  - One executable per `test/**/*.cpp`; they're auto-discovered.
  - Tests that load a DB must reset it on exit. Copy the `ResetDatabase` RAII
    struct from `test/schrodinger/sketcher/widget/test_monomer_tool_widget.cpp`.
- **Playwright (WASM) tests.** Every step that changes the GUI (Steps 2–6)
  must add Playwright coverage. It must also update any existing Playwright
  test whose behavior this work intentionally changes, including tests added
  by earlier steps.
  - **Location:** suites are in `test/wasm/e2e/`.
    - Helpers: `e2e_helpers.js` and `monomer_helpers.js`. `loadDatabase(page,
      fixture)` uses the import menu.
    - Fixtures: `test/wasm/e2e/fixtures/` — `chem_monomers.json`,
      `na_monomers.json`, `long_symbol.json`, `chembl_monomers_no_id.json`,
      `empty_monomers.json`, `invalid_monomers.json`,
      `native_monomer_definitions.json`.
    - Add one mixed fixture with peptide, nucleic-acid and CHEM monomers.
      Include backbone and terminal peptides, several analogs, a None analog
      and a long symbol. This lets filter tests run against one DB.
  - **The bridge** (`src/app/playwright_test_bridge.cpp`) locates widgets by
    objectName and reports their state. Tests then drive the app with real
    mouse and keyboard events.
    - It can't yet read table rows or combo-popup items.
    - Step 3 adds table support: row count, visible headers, the selected
      row, cell text for a row, and a selector that yields a row's rect
      after scrolling it into view.
    - Step 4 adds checkable-combo support: the item check states, plus a
      selector for an item in an open combo dropdown.
    - Follow the bridge's stated philosophy: close only the gap Playwright
      can't, and keep driving through real events.
  - **Stable objectNames.** Give every new widget one. The names here are
    the contract the tests use:

    | Widget | objectName |
    |---|---|
    | Side-bar pair | `monomer_db_last_used_btn`, `monomer_db_btn` |
    | Popup | `monomer_db_popup` |
    | Popup contents | `monomer_db_search_le`, `monomer_db_table`, `monomer_db_match_count_lbl` |
    | Popup links and close | `monomer_db_load_link`, `monomer_db_close_link`, `monomer_db_close_btn` (×) |
    | Filter popup | `monomer_db_filter_btn`, `monomer_db_filter_popup`, `polymer_type_combo`, `monomer_type_combo`, `natural_analog_combo`, `filter_clear_all_link`, `filters_applied_lbl`, `filter_close_btn` |
    | Folder entry in long-press popups | `analog_monomer_db_btn` |
    | Right-hand pane (Step 6) | `monomer_db_description_lbl`, `monomer_db_image_lbl` |

  - **Running the tests needs a WASM artifact.** WASM builds are remote, and
    the user runs them.
    1. The sub-agent writes the tests and checks them for syntax.
    2. The sub-agent asks the user for an artifact built from the step's
       changes, including any bridge changes.
    3. From `test/wasm`, the sub-agent runs:
       `SKETCHER_WASM_APP_DIR=<artifact dir> npx playwright test
       e2e/<files>`.
    4. The sub-agent iterates until the tests pass.

    A step isn't done until its Playwright tests pass against such an
    artifact.
  - **Screenshot references:** regenerate only those that the step
    intentionally changes, such as side-bar layout. List each regenerated
    snapshot in the hand-off summary.
  - **Coverage table:** update `test/wasm/RESOLVED_CASE_COVERAGE.md` rows
    for any suite whose coverage changes.
- **When finishing**
  - Run clang-format on changed C++, and prettier on changed JS (see
    `CLAUDE.md`).
  - Stage new, deleted and renamed files. Ask the user, because the sandbox
    blocks `.git/index.lock`.
  - Don't commit.

---

## Step 1: Filtering logic and table model

**Goal:** all of the popup's non-visual logic, with unit tests and no UI
change.

### 1a. `getCustomMonomers()` and `getCoreMonomers()` (rdkit_extensions)

- **Add the methods** to `MonomerDatabase`:
  - `[[nodiscard]] std::vector<MonomerInfo> getCustomMonomers() const;`
    returns every monomer in the custom DB, of all polymer types. It returns
    an empty vector when no custom DB is loaded.
  - `[[nodiscard]] std::vector<MonomerInfo> getCoreMonomers() const;`
    returns every monomer in `m_core_monomers_db`.
- **Refactor, don't copy.** Refactor the private `getMonomers(ChainType,
  bool)` so that all of these share the row-reading loop (prepare, step,
  `assign_monomer_info`). Parameterize it by which DB handle(s) to read, so
  that Step 5 can add a core-only analog query without copying it.
- **Tests** go in `test/schrodinger/rdkit_extensions/test_monomer_database.cpp`:
  - `getCustomMonomers()` is empty when only the core DB exists;
  - it returns all custom monomers across polymer types;
  - `getCoreMonomers()` returns the 70 core monomers whether or not a custom
    DB is loaded;
  - `getCustomMonomers()` is empty again after `resetMonomerDefinitions()`.

### 1b. Filter, sort and pure functions (Qt-free)

**New files:** `src/schrodinger/sketcher/model/monomer_database_filter.{h,cpp}`.
- The header must include no Qt headers, directly or indirectly.
- The implementation must not use Qt. The `.cpp` may include
  `sketcher_model.h`, which is Qt, for the monomer tool constants
  (`AminoAcidTool`, `AMINO_ACID_TOOL_TO_RES_NAME` and so on); those stay in
  `sketcher_model.h`.

**Proposed API.** The sub-agent may refine names.

```cpp
enum class MonomerDbPolymerType : unsigned {
    NONE = 0, PEPTIDE = 1 << 0, NUCLEIC_ACID = 1 << 1, CHEM = 1 << 2,
    ALL = PEPTIDE | NUCLEIC_ACID | CHEM,
};
// operator|, operator&, has_flag(), count_flags() helpers

enum class MonomerTypeFilter { ANY, BACKBONE, TERMINAL };
enum class NaturalAnalogFilter { ANY_OR_NONE, ANY, NONE, SPECIFIC };
enum class MonomerDbColumn { SYMBOL, NAME, POLYMER_TYPE, NATURAL_ANALOG };

struct MonomerDbSort {
    MonomerDbColumn column = MonomerDbColumn::SYMBOL;
    bool ascending = true;
    bool operator==(const MonomerDbSort&) const = default;
};

class MonomerDatabaseFilter
{
  public:
    // Default-constructed == no filtering: ALL types, ANY, ANY_OR_NONE
    MonomerDatabaseFilter() = default;
    explicit MonomerDatabaseFilter(MonomerDbPolymerType polymer_types);

    // Setters store the user's choice (see "Values hidden by applicability
    // are remembered" under Design decisions). setPolymerTypes() never
    // clears a remembered value, except that it forgets the natural analog
    // when that combo becomes applicable for a different polymer type than
    // the one the value was chosen for, and forgets the monomer type when
    // the natural analog becomes applicable for Nucleic acid.
    void setPolymerTypes(MonomerDbPolymerType polymer_types);
    void setMonomerType(MonomerTypeFilter monomer_type);
    // Records the currently applicable polymer type alongside the value.
    // Precondition: isNaturalAnalogApplicable() (assert)
    void setNaturalAnalog(NaturalAnalogFilter analog,
                          std::string specific = {});

    // Effective values: what is actually applied. They return ANY /
    // ANY_OR_NONE while the combo is inapplicable, and the remembered value
    // otherwise. The remembered values themselves stay private.
    MonomerDbPolymerType getPolymerTypes() const;
    MonomerTypeFilter getMonomerType() const;
    NaturalAnalogFilter getNaturalAnalog() const;
    const std::string& getSpecificNaturalAnalog() const; // "" unless SPECIFIC

    // CHEM never affects applicability:
    bool isMonomerTypeApplicable() const;   // PEPTIDE set, NUCLEIC_ACID not
    bool isNaturalAnalogApplicable() const; // exactly one of PEPTIDE and
                                            // NUCLEIC_ACID set
    std::vector<std::string> getNaturalAnalogChoices() const;

    // "Filtering" is relative to the default-constructed (show-everything)
    // filter; these drive the count, bold combos and the orange button
    bool isPolymerTypeFiltered() const;
    bool isMonomerTypeFiltered() const;     // false when inapplicable
    bool isNaturalAnalogFiltered() const;   // false when inapplicable
    int getNumFiltersApplied() const;
    bool isFiltering() const;               // getNumFiltersApplied() > 0

    std::string getDescription() const;
    bool matches(const rdkit_extensions::MonomerInfo& info) const;
    // Compares the full state, including remembered values
    bool operator==(const MonomerDatabaseFilter&) const = default;
};

// The table's source list: every custom monomer, plus each core monomer whose
// natural analog is "None" (type-specific; see has_no_natural_analog) unless a
// custom monomer has the same symbol and polymer type
std::vector<rdkit_extensions::MonomerInfo> get_monomer_database_table_monomers(
    std::vector<rdkit_extensions::MonomerInfo> custom_monomers,
    const std::vector<rdkit_extensions::MonomerInfo>& core_monomers);

// The type-specific "None" rule: empty, X for PEPTIDE, N for RNA; always true
// for CHEM. Shared by the source-list builder and NaturalAnalogFilter::NONE.
bool has_no_natural_analog(const rdkit_extensions::MonomerInfo& info);

// Returns indices into all_monomers of the matching rows, in sorted order
std::vector<std::size_t> filter_and_sort_monomers(
    const std::vector<rdkit_extensions::MonomerInfo>& all_monomers,
    const MonomerDatabaseFilter& filter, const MonomerDbSort& sort);

// Returns the subset of rows (indices into all_monomers) whose symbol or name
// contains text, case-insensitively, preserving order
std::vector<std::size_t> filter_by_search_text(
    const std::vector<rdkit_extensions::MonomerInfo>& all_monomers,
    const std::vector<std::size_t>& rows, std::string_view text);
```

**Semantics:**
- **Polymer type**
  - Map `MonomerInfo::polymer_type` with `rdkit_extensions::toChainType`:
    PEPTIDE→PEPTIDE, RNA→NUCLEIC_ACID, CHEM→CHEM.
  - Rows with a missing or unparseable type never match.
  - `NONE` matches nothing. The UI never produces it (Step 4), but the API
    stays total.
- **Applicability** is defined once, by the methods above. Everything else
  calls them: `matches()`, the count, the description, and Step 4's combo
  enabling.
- **Effective vs remembered values.** `matches()`, the count, the
  `is*Filtered()` methods, the description and the combo display all use the
  effective getters, never the remembered values directly. Clear All and the
  page defaults are new filter objects, so they carry no memory.
- **Equality.** `operator==` compares the full state, so two filters with
  the same effective values but different remembered values are unequal.
  That's intended. Nothing in this plan needs `==` in production code; it's
  for tests, and optionally for skipping a re-filter when nothing changed (a
  memory-only change then causes one cheap extra re-filter). Tests that mean
  "applies the same filtering" compare the effective getters instead.
- **Monomer type:** a case-insensitive comparison with `Backbone` /
  `Terminal`. CHEM rows match only `ANY`.
- **CHEM and the analog filter:** CHEM rows match only `NONE` and
  `ANY_OR_NONE`. They have no natural analog, so `has_no_natural_analog()`
  is true for them.
- **Natural analog:** as described under "Design decisions".
  - Don't hard-code `"X"` and `"N"`. Find and reuse the existing constants,
    for example `AMINO_ACID_TOOL_TO_RES_NAME.at(AminoAcidTool::UNK)` and the
    NA `N` entry, or an rdkit_extensions constant.
  - Peptide choices come from `AMINO_ACID_TOOL_DISPLAY_ORDER` minus UNK,
    reusing the existing tables in `sketcher_model.h` rather than copying
    them.
- **Description:** follow the table under "Design decisions". Reuse an
  existing one-letter→three-letter amino-acid map for "Cys". Search the code
  before adding one.
- **Sort**
  - Case-insensitive and stable.
  - Tie-breaks: symbol, then polymer type.
  - Missing values sort last in ascending order and first in descending
    order (the whole order is reversed).
  - Polymer type sorts by its display text.
- **Search:** a case-insensitive substring match on symbol or name. Empty
  text returns the input unchanged.
  - The spec's "hydro" example matches "Hydroxyisoleucine" and
    "Me_Phe(a,b-dehydro)", so this is a substring search, not a prefix
    search.

**Tests** go in `test/schrodinger/sketcher/model/test_monomer_database_filter.cpp`.
Use hand-built `MonomerInfo` vectors, so no DB is needed. Cover:
- `get_monomer_database_table_monomers`:
  - it keeps every custom monomer;
  - it keeps core monomers with X, N or empty analogs only, by type, so a
    core peptide with analog `N` (Asn) is excluded;
  - a custom monomer replaces a core one with the same symbol and polymer
    type, but not one of a different polymer type;
- every polymer-type combination, including NONE and ALL;
- applicability for every combination, including Structure combined with
  Peptide or Nucleic acid, and that inapplicable combos have effective
  values Any / Any or None;
- remembered values:
  - {Peptide} + Backbone + C, then {Peptide, Nucleic acid}, then {Peptide}
    restores Backbone and C, and the inapplicable middle state matches,
    counts and describes as Any / Any or None;
  - the same through {Peptide, Structure} → {Structure} → {Peptide,
    Structure};
  - a peptide analog (C, None or Any) is forgotten when the analog applies
    again for Nucleic acid, through either toggle order ({Peptide, Nucleic
    acid} → {Nucleic acid}, or {Structure} → {Nucleic acid, Structure}), and
    stays forgotten on returning to {Peptide};
  - the reverse, from a nucleic-acid analog to Peptide;
  - Monomer type survives {Peptide, Nucleic acid} and {Structure}, but is
    forgotten on reaching {Nucleic acid} or {Nucleic acid, Structure}, and
    stays Any on returning to {Peptide};
  - `setNaturalAnalog` records the polymer type it was applicable for;
  - `==` sees remembered values, and the effective getters don't;
- CHEM rows against every monomer-type and analog value;
- None/Any with `X`, `N` and empty for both peptides and nucleic acids,
  including that peptide `N` (Asn) is **not** None;
- case-insensitive monomer type;
- every sort column and direction, with ties and missing values;
- search on symbol vs name;
- the count, `isFiltering()`, the per-combo `is*Filtered()` and the
  description for every row of the description table and both page
  defaults.

### 1c. `MonomerDatabaseTableModel` (Qt)

**New files:** `src/schrodinger/sketcher/widget/monomer_database_table_model.{h,cpp}`,
a `QAbstractTableModel` subclass. It's a thin adapter and contains no
filtering or sorting logic.

- **`setMonomers(...)`:**
  - takes `std::shared_ptr<const std::vector<MonomerInfo>> all_monomers` and
    `std::vector<std::size_t> rows`;
  - wraps the update in `beginResetModel`/`endResetModel`.

  Sharing the cache avoids both copies and dangling references.
- **Columns:**
  - The columns are Symbol, Name, Type and Analog (`MonomerDbColumn` order).
  - Data uses `Qt::DisplayRole`. Analog is centered and blank for None.
  - Header labels come from `headerData`.
- **Sorting is delegated:**
  - Override `sort(int column, Qt::SortOrder)` to emit
    `sortRequested(MonomerDbSort)`. It doesn't sort.
  - The popup stores the sort and re-runs the pure functions.
- **Helpers:**
  - `rdkit_extensions::MonomerID getMonomerID(int row) const`;
  - `std::optional<int> getRow(const MonomerID&) const`;
  - `const MonomerInfo& getMonomerInfo(int row) const`.
- **Tests** go in
  `test/schrodinger/sketcher/widget/test_monomer_database_table_model.cpp`.
  Cover:
  - row and column counts;
  - display data, including blank None analogs and Type text;
  - header labels and reset;
  - row↔ID lookups;
  - the sort signal.

**Done when:** the new tests and `test_monomer_database` pass, and the UI is
unchanged. No Playwright work is needed for this step.

---

## Step 2: Tool-state model and shared popup positioning

**Goal:** the model and tool-state changes the popup depends on, plus the
shared popup positioning, with no new widgets.
- The Unclassified buttons and their popups keep working, now on top of the
  new keys and the new activation function. Step 3 replaces them.
- User-visible changes after this step:
  - committing a mutation from an Unclassified popup also stores the monomer
    in that page's last-used slot, without changing the draw tool;
  - the AA/NA page buttons follow `MONOMER_TOOL_TYPE` under every draw tool;
  - a database change that removes a remembered DB monomer repairs the tool
    state;
  - long-press popups use the new placement order and stay inside the
    available area.

### 2a. Remove the markers and split the model key

- **Remove `AminoAcidTool::UNCLASSIFIED` and `NucleicAcidTool::UNCLASSIFIED`.**
  (They were first renamed to `MONOMER_DB`, then removed once the per-page
  record of the last monomeric draw tool made them redundant; see "Buttons
  and tool state".) The Unclassified buttons are no longer in the
  standard-button bimaps. Trace every use:
  - `sketcher_model.h`;
  - `scene.cpp` (around line 766);
  - `sketcher_widget.cpp` (`nucleic_acid_tool_to_monomer_type`, and the
    `NUCLEIC_ACID_SYMBOL` mutation guard);
  - `sketcher_side_bar.cpp` (`updateCheckState`);
  - `monomer_tool_widget.cpp`;
  - `monomer_symbol_popup_utils.cpp`;
  - the C++ tests.

  Leave code that only serves the Unclassified popups for Step 3 to
  remove.
- **Split the key.** Replace `ModelKey::MONOMER_DB_MONOMER` with
  `AMINO_ACID_DB_MONOMER` and `NUCLEIC_ACID_DB_MONOMER`.
  - Both default to an empty `MonomerID`.
  - Add `SketcherModel::getMonomerDbMonomer()`.
  - Update `sketcher_model.cpp` (the default-value table and the key list
    around line 94), `Scene::getNewSceneTool()`,
    `Scene::onModelValuesChanged()` (both new keys), the
    `SketcherWidget::applyModelValuePingToTargets` mutation case (handle
    both keys), `monomer_symbol_popup_utils.cpp` and the tests.
  - Make the Unclassified buttons read and write their page's key, replacing
    any per-button storage such as `getMonomerDbMonomerForButton`.

### 2b. One activation function

Replace `MonomerToolWidget::activateMonomerDbMonomer(QAbstractButton*)` with
`activateMonomerDbMonomer(const MonomerID&)`. It implements "Choosing a
monomer switches the side-bar page" from "Design decisions":
- It picks the target page: PEPTIDE→AMINO_ACID, RNA→NUCLEIC_ACID,
  CHEM→the current page.
- With a canvas selection, it sets that page's `*_DB_MONOMER` (which mutates
  the selection) and `MONOMER_TOOL_TYPE` in one `setValues` call. It doesn't
  set the draw tool: mutation updates only the last-used slot. Today's code
  only pings the key in this case; storing it is the intended change.
- Otherwise, it sets `DRAW_TOOL`, `MONOMER_TOOL_TYPE` and the page's
  `*_DB_MONOMER` in one `setValues` call. It never sets the page's
  `AMINO_ACID_TOOL` / `NUCLEIC_ACID_TOOL`.
- In this step the Unclassified popups call it. In Step 3 the table rows and
  the last-used button do.

### 2c. Page synchronization and restoration

- **`updateCheckedButton()`:** synchronize the AA/NA page buttons and
  `amino_or_nucleic_stack` from `MONOMER_TOOL_TYPE` regardless of
  `DRAW_TOOL`. Move this synchronization out of the current
  monomer-drawing-tool conditional, so it also runs for Select, Move/Rotate
  and the connection tools.
- **`onAminoOrNucleicBtnClicked`:** with a canvas selection, it still only
  sets `MONOMER_TOOL_TYPE`. Otherwise it activates
  `getDrawToolToRestore(page)`: the DB monomer through the activation
  function, or any other tool with `setValues`.
- **`SketcherSideBar::updateCheckState`:** the atomistic→monomeric toggle
  also activates `getDrawToolToRestore(page)`.
- Restoring a DB tool requires a non-empty DB slot.

### 2d. Database-change repair in `SketcherModel`

Implement "`SketcherModel` repairs its own DB state when the database
changes" from "Design decisions":
- the subscription and its `scoped_connection` member;
- validity through Step 1's `get_monomer_database_table_monomers()`;
- the per-page repair rules;
- one update that emits `valuesChanged` but no `valuePinged`, through a
  private helper that shares code with `setValues()`. Don't change
  `SketcherModel`'s public API for this.

Remove any equivalent repair from `MonomerToolWidget`. If the current
Unclassified code does something on reload that these rules don't cover,
tell the user rather than silently keeping or dropping it.

### 2e. Shared popup positioning

- Extract `show_popup_next_to_widget` and the clamping helper (see "Shared
  helpers"), following "Popup bounds and resizing" for placement and
  clamping. DB-popup shrinking isn't part of this step.
- `ToolButtonWithPopup::showPopup()` calls `show_popup_next_to_widget`.
- `ModalDialog::moveOntoScreen()` calls the clamping helper, with its
  behaviour unchanged.

### C++ tests

Put model tests in `test_sketcher_model.cpp`, widget tests in
`test_monomer_tool_widget.cpp`, and positioning tests next to the helper's
existing tests (or in a new `test_widget_utils.cpp`).
- **Keys:** each page stores its own slot, and `getMonomerDbMonomer()`
  returns the current page's.
- **Activation without a canvas selection:**
  - it sets the draw tool, the page and the page's slot, and leaves the
    page's `AMINO_ACID_TOOL` / `NUCLEIC_ACID_TOOL` alone;
  - a peptide switches to the AA page, an RNA monomer to the NA page, and
    CHEM stays on the current page (call the function directly, since no UI
    can reach a cross-page monomer yet);
  - the monomer is stored in that page's key only.
- **Activation with a canvas selection:**
  - it mutates once, with one undo entry;
  - it keeps the draw tool;
  - it switches the model and visible page, including the checked page
    button, under both Select and Move/Rotate;
  - it updates that page's last-used slot;
  - it doesn't ping or change the page's tool key;
  - a type-mismatched monomer switches the page and updates the slot without
    mutating or adding an undo entry.
- **Page switching:**
  - each page restores its own DB monomer;
  - a page whose last tool was a standard button restores that button;
  - an empty slot never restores a DB tool.
- **`SketcherSideBar`:** an atomistic↔monomeric toggle restores the DB tool.
- **Database-change repair:**
  - a still-valid slot is kept;
  - removal on the active page with the DB tool active blanks the slot and
    sets `DRAW_TOOL` to `MONOMER`, which activates the page's last standard
    button, without changing either page's tool or symbol keys;
  - removal on an inactive page, and while atomistic mode is active, blanks
    the slot, so returning to that page restores its standard tool;
  - removal with a canvas selection causes no mutation and no undo entry, and
    keeps Select or Move/Rotate;
  - the page's standard tool and symbol are kept;
  - unloading a custom override of a core monomer with an analog (`meG`)
    clears a slot holding it;
  - the repair emits `valuesChanged` but no `valuePinged`.
- **Unclassified buttons:** rewrite the existing `unclassified_monomers` and
  `unclassified_chem_monomers` cases, and the related cases in
  `test_sketcher_widget.cpp` and the side-bar tests, for the new keys. They
  keep covering CHEM from either page, mutation of a selection, and per-page
  memory.
- **Positioning:** placement below, above and on the side with more room;
  clamping at each edge; an area smaller than the popup keeps top/left in
  bounds with overflow to the right/bottom. `ModalDialog` positions exactly
  as before.

### Playwright

- Run the suites that touch the Unclassified buttons and long-press popups
  (grep `test/wasm/e2e` for `unclassified`, `clickPopupTool` and
  `choosePopup`). Update cases whose behaviour this step intentionally
  changes, such as per-tab memory after a mutation.
- **New cases** (in `monomer_database_regressions.test.js`, or a new suite
  if that reads better):
  - mutating a selection from an Unclassified popup, then switching pages
    and back, shows that page's remembered DB monomer;
  - choosing a DB monomer, then loading a database without it, falls back to
    the standard tool: the page's Ala (or RNA nucleotide) button is checked
    and clicking the canvas draws it;
  - a long-press popup opened near the bottom or right of a small viewport
    stays inside the canvas.

**Done when:** the C++ and Playwright tests above pass, and there's no new
widget yet.

---

## Step 3: Side-bar buttons and the Monomer Database popup (no filter popup)

**Goal:**
- The shared button pair replaces the Unclassified buttons.
- The DB button opens the popup, which has:
  - a search box;
  - the table;
  - a match count;
  - Close, and Load Database… / Update Database….
- Choosing a row activates or commits that monomer through Step 2's
  activation function.
- The filter is the page default, with no way to change it yet.
- There's no filter button, description or image yet.

### 3a. Remove the Unclassified buttons

- **Delete the buttons.** Remove `aa_unclassified_btn` and
  `na_unclassified_btn` from `ui/monomer_tool_widget.ui`.
- **Delete the Unclassified-only code in `MonomerToolWidget`:**
  - the X/N + CHEM popup construction;
  - the `-1` enum-item handling;
  - anything Step 2 kept only for these buttons.

  Rework the remaining Unclassified branches of `updateCheckedButton`,
  `onAminoOrNucleicBtnClicked` and `onNucleicAcidClicked` for the shared
  pair (3d). Leave the normal analog-popup construction alone; Step 5
  replaces it.
- **No popups for `unk_btn` / `na_n_btn`.** `ac` and `am` stay reachable
  through the table.
- **Rewrite the affected C++ tests.** Step 2's Unclassified cases move to
  the new pair: CHEM reachable from either page, mutation of a selection,
  each page remembering its own DB monomer, and the atomistic↔monomeric
  toggle restoring the DB tool.

### 3b. Shared helpers

Extract `paint_text_with_fade_out` / `elide_text()` and
`load_monomer_database_from_file` (see "Shared helpers"). Then make the
original call sites use them:
- `AbstractMonomerItem::paint()`;
- `SketcherTopBar::onLoadMonomerDatabaseClicked()`.

Keep the existing monomer-item rendering and top-bar tests green.

### 3c. New widgets

- **Icons:**
  - Add `icons/monomer_db.svg` to `src/schrodinger/sketcher/sketcher.qrc`.
  - Copy `filter-menu_lb.png`, `filter-menu_lb_act.png`,
    `filter-menu_lb_filtering.png` and `filter-menu_lb_hvr.png` from
    `F:\repos\src\mmshare\python\modules\schrodinger\ui\qt\standard\icons\icon_data\filter-menu`
    into `src/schrodinger/sketcher/icons/`.
  - Add them to the qrc now, ready for Step 4.
- **Last-used button**
  - Files: `widget/monomer_database_last_used_button.{h,cpp}`, a
    checkable `QToolButton` subclass, objectName `monomer_db_last_used_btn`.
  - **Display:** it shows the symbol of `getMonomerDbMonomer()`, the
    current page's DB monomer. It updates when the page changes.
    - It uses `paint_text_with_fade_out` when the symbol is too wide, like
      the monomer graphics items.
    - The tooltip is the monomer's name.
  - **When blank:** it looks like empty space and clicks do nothing. There's
    no hover or pressed feedback; disabling it while blank is the simplest
    way.
  - **Style:** match the neighbouring side-bar buttons.
- **DB button:** a plain (non-checkable) `QToolButton`, objectName
  `monomer_db_btn`.
  - It shows the `monomer_db.svg` icon and a popup-indicator arrow, matching
    the spec's "NMA [folder▾]" look.
  - Clicking it while the popup is open closes the popup. It doesn't
    close and reopen it.
- **DB popup:**
  - Files: `ui/monomer_database_popup.ui` and
    `widget/monomer_database_popup.{h,cpp}`.
  - It's a `SketcherViewWithWasmOutlineFix` with `Qt::Popup`, modeled on
    `FileSaveImagePopup`.
  - It holds a `const SketcherModel*`, passed once at construction or
    through a setter. It reads tool state and `hasActiveSelection()` from it
    and listens to its `selectionChanged`.
  - **Header:** a `CustomTitleBar` (`dialog/custom_title_bar.h`), the title
    bar that `ModalDialog` uses on WASM, with the title "Monomer Database"
    and an × (`monomer_db_close_btn`). Extend it with an optional close
    button and an option to turn off dragging, since the popup stays next to
    its button. A `Qt::Popup` never gets native window chrome, and switching
    to a real window (e.g. `Qt::Tool`) would lose the popup behaviour this
    plan relies on, so:
    - **WASM:** the dark appearance, as in the mockup and the WASM dialogs;
    - **Desktop:** the native-like appearance (`NATIVE_LIKE`): the system
      palette's window and text colors, and the platform style's close icon
      (`QStyle::SP_TitleBarCloseButton`).
  - **Content:**
    - **Search box:** a `QLineEdit` with a leading search icon,
      `setClearButtonEnabled(true)` and live filtering on `textChanged`.
      Typing in it must not trigger Sketcher keyboard shortcuts.
    - **Table:** a small `QTableView` subclass with:
      - single selection, selecting rows;
      - the `selectionCommand()` override for mutation mode (see "The popup
        has two modes");
      - the current-row highlight (see "The current row has its own subtle
        highlight");
      - the Enter/Return/Space handling (see "Enter/Return and Space");
      - no editing;
      - no vertical header;
      - fixed row heights;
      - sorting enabled through the model's `sortRequested`. The initial
        sort is the `MonomerDbSort` default, Symbol ascending. Set the
        header's sort indicator from the stored `MonomerDbSort` (with
        `setSortIndicator`) **before** calling `setSortingEnabled(true)`.
        Enabling sorting immediately sorts by the current indicator, and
        Qt's default indicator is likely column 0 descending, which would
        request a descending sort and mismatch the stored one. Keep the
        indicator in sync whenever the stored sort changes, with signals
        blocked so syncing doesn't request another sort;
      - Name column stretched;
      - Type column hidden when the filter allows one polymer type.

      It paints "No monomers match search & filter criteria" (gray italics)
      when it has zero rows.
    - **Match count:** "N matches" ("1 match").
    - **Room** on the right for Step 4's filter button and Step 6's
      description and image.
  - **Bottom row:**
    - `monomer_db_load_link` on the left, with the text "Load Database…"
      when no custom monomers are loaded or "Update Database…" otherwise;
    - "Close" (`monomer_db_close_link`, `TEXT_LINK_STYLE`) on the right.
  - **Load-link visibility:** it follows `setLoadMonomerDatabaseVisible`.
    The path is `SketcherWidget` → `SketcherSideBar` → `MonomerToolWidget` →
    popup. Store the flag so that it applies even before the popup exists.
  - **Load-link activation:** hide nested popups and this popup before
    calling the shared loader with a stable owning widget/window as its
    parent. Preserve search/filter/sort, keep processing database-change
    notifications while hidden, and never reopen automatically after the
    chooser or result dialog finishes. Follow the lifecycle under "Design
    decisions".
  - **Data:**
    - Cache `get_monomer_database_table_monomers(getCustomMonomers(),
      getCoreMonomers())` in a `shared_ptr<const vector>`.
    - Refresh the cache from its own `subscribeToChanges` subscription. Read
      the database directly; don't depend on `SketcherModel`'s repair having
      run first (see "Database-change listeners are independent").
    - The load link says "Load Database…" when `getCustomMonomers()` is
      empty and "Update Database…" otherwise.
    - After a refresh, re-filter, re-sync the selection and update the
      load-link text.
    - Filter and sort changes re-run `filter_and_sort_monomers`.
      `textChanged` re-runs only `filter_by_search_text` on the cached
      filtered rows.
    - Before every model reset, save the current row's `MonomerID`, not its
      index or row number. Afterwards, re-sync the selection and restore the
      current index by ID if it is still visible. A model-selected row takes
      precedence; otherwise preserve the browsing row. If it disappeared or
      was filtered out, clear the current index instead of choosing another
      monomer by row number. Explicitly refresh the preview after this
      process.
  - **API:**
    - `showFromButton(QWidget* anchor, MonomerToolType page)` positions the
      popup with `show_popup_next_to_widget`. Before showing, it applies the
      reset rule from "Design decisions" and reads the mode.
    - On every close (×, Close, outside click, Escape, DB button, load/update
      link, a mode change), the popup records the current
      `MONOMER_TOOL_TYPE`. Hook `hideEvent` so that no path is missed.
    - **Mode change:** on `selectionChanged`, if `hasActiveSelection()` no
      longer matches the value read on opening, hide the popup.
    - Signal `monomerChosen(MonomerID)` fires:
      - in tool mode, on user-driven row changes (mouse or keyboard), and on
        Enter/Return or Space;
      - in mutation mode, only on an explicit click on a valid row,
        Enter/Return, or Space. Keyboard commits use the current index.
      - Navigation in mutation mode, model resets and programmatic
        selection/current-index changes never fire it. Avoid firing twice
        through both key events and activation signals.
    - `syncSelectionToModel()` applies the selection rule from "Design
      decisions":
      - it uses `QSignalBlocker` on the selection model;
      - it calls `scrollTo` for the selected row;
      - when clearing selection, use `clearSelection()` rather than `clear()`
        so the browsing current index survives;
      - a model ID that isn't in the cache selects nothing;
      - refresh the preview explicitly after blocked updates; normal browsing
        also refreshes it on current-index changes.

      In mutation mode, a commit mutates the selection and leaves the
      committed row current, with no table selection. Its preview remains
      visible. Clicking it again or pressing Enter/Return or Space re-applies
      it, even though neither the current index nor the last-used ID changed.
  - **Clicks outside the popup** dismiss it and, on native builds, pass
    through, except over the originating button. On WASM they only dismiss
    it (see "Clicking outside the popup" under Design decisions). Install
    `configure_button_popup` rather than globally setting `WA_NoMouseReplay`
    or duplicating its event filter.
  - **Bounds:** follow "Popup bounds and resizing" under Design decisions
    (SKETCH-2920). Shrink the table viewport to keep the popup within the
    available area whenever its minimum layout fits; Step 6 adds preview
    scaling. Recalculate while open on available-area changes. If the
    minimum layout cannot fit, keep the top-left reachable and allow
    right/bottom overflow, without adding whole-popup scrolling.

### 3d. Layout and wiring in `MonomerToolWidget`

- **Layout.** In `ui/monomer_tool_widget.ui`, add a horizontal row between
  `line` and `custom_monomer_btn`: `[monomer_db_last_used_btn]
  [monomer_db_btn]`. Neither button joins a `QButtonGroup`.
- **Row choice:** `monomerChosen` calls Step 2's
  `activateMonomerDbMonomer(const MonomerID&)`.
- **DB-button click:** show the popup for the current `MONOMER_TOOL_TYPE`.
  It doesn't activate anything.
- **Last-used click:**
  - When blank, it does nothing.
  - Otherwise it calls `activateMonomerDbMonomer(getMonomerDbMonomer())`.
    That keeps the current page, since the stored monomer belongs to it.
- **`updateCheckedButton()`:**
  - Check `monomer_db_last_used_btn` exactly when
    `draw_tool == MONOMER_DB_MONOMER` and there's no selection. In that
    case, uncheck the page groups.
  - Refresh the last-used text from `getMonomerDbMonomer()`.
  - Call `popup->syncSelectionToModel()`.
- **Enabled state:** both buttons are enabled whenever the monomeric tools
  are. The only exception is the blank last-used button.
  - If `updateWidgetsEnabled()` would re-enable a blank last-used button,
    fix it there.

### C++ tests

Add these to `test_monomer_tool_widget.cpp`, or put popup-only behaviour in a
new `test_monomer_database_popup.cpp`.
- **Load link:**
  - With no custom DB, the table shows `ac` and `am`, and the link reads
    "Load Database…".
  - After loading a DB, it reads "Update Database…".
  - After `resetMonomerDefinitions()`, it reads "Load Database…" again.
  - `setLoadMonomerDatabaseVisible(false)` hides the link.
  - Load/update hides the popup before the chooser opens and records the page
    at close; success, cancellation, partial import and failure leave it
    closed.
  - Reopening on the same page preserves search and sort and uses refreshed
    data after a load. In Step 4, also verify that the filter is preserved.
    Changing page while hidden still invokes the normal page-default reset.
- **Table contents:**
  - The table holds custom monomers plus core monomers with no natural
    analog, with custom winning on a symbol clash.
  - The AA default shows peptide and CHEM rows. The NA default shows nucleic
    acid and CHEM rows. The Type column is visible.
- **Reset rule (keyed on the page at close):**
  - Reopening on the same page keeps the search text.
  - Opening on a page that differs from the page at the last close clears
    the search text and resets the filter.
  - Cross-page row-choice/reopening tests belong to Step 4, when the filter
    controls make an RNA row reachable from the AA page.
- **Search:**
  - Search narrows the rows, and the match count is correct.
  - Zero matches shows the placeholder.
- **Sorting:**
  - On first opening, the rows are in Symbol-ascending order, and the
    header's indicator shows Symbol ascending.
  - Clicking a header sorts by that column, and the indicator and the
    stored sort agree; reopening the popup keeps both.
- **Choosing a row in tool mode:**
  - It sets the tool and checks and labels the last-used button.
  - The popup stays open.
- **Opening the popup** doesn't change the tool.
- **Last-used button:**
  - Clicking it reactivates the monomer after another tool was chosen.
  - A blank button does nothing.
- **Selection mirroring:**
  - A model change made elsewhere moves the table selection.
  - The selection is cleared for a non-DB tool and for a monomer that isn't
    in the table.
  - A model ID missing from the popup's cache selects nothing.
- **Mutation mode:**
  - it mutates only on click, Enter/Return or Space, once per explicit
    commit;
  - keyboard navigation changes only the current index, with no mutation,
    undo entry, last-used update or page switch;
  - commit keys in the search box don't commit a row;
  - the committed row stays current, with the table selection empty, and
    the same row can be committed again without a row change;
  - the table never gains a selected row, even on click (`selectionCommand`).
- **Mode is fixed while open:**
  - a commit doesn't close the popup;
  - Ctrl+A with focus in the search box selects its text and doesn't select
    the canvas; with focus in the table it does nothing;
  - a programmatic change that adds a canvas selection while the popup is
    open in tool mode closes it; one that clears the selection in mutation
    mode closes it too; a change that keeps a selection non-empty doesn't.
- **Enter/Return and Space:**
  - in tool mode, on a current row that isn't the active monomer (for
    example after reopening with a standard tool active), they activate it;
  - they never close the popup in either mode.
- **Current-row highlight:** a current row that isn't selected paints the
  highlight color; the selected row paints the selection color.
- **Current index across resets:** preserve by ID after sorting/filtering
  or DB refresh when still visible; clear when removed/filtered out. No
  reset or synchronization commits a mutation.
- **DB reload while the popup exists:** the table refreshes, and a removed
  DB monomer blanks the last-used button (the repair itself is tested in
  Step 2).
- **`ac` / `am`** can be chosen from the table and drawn.
- **Long symbol:** a long symbol takes the fade path.
- **Bounds:** placement below/above the anchor, clamping at each edge,
  shrinking the table viewport, and resizing the available area while open.
  Include an area smaller than the minimum layout: top/left remain in bounds
  and any unavoidable overflow is confined to right/bottom.

### Playwright

- **Bridge:** add the table support described under Shared context.
- **Shared helper:** add `chooseFromMonomerDatabase(page, symbol, {search})`
  to `monomer_helpers.js`. Every later test that needs a custom monomer
  should use it.
- **New suite `e2e/monomer_database_popup.test.js`:**
  - opening and closing via ×, Close, the DB button and an outside click;
  - an outside click on the canvas dismisses the popup without placing a
    monomer. This is WASM's behaviour, unlike native builds where the click
    also reaches the canvas (see "Clicking outside the popup" under Design
    decisions). Assert it, so that a change in Qt's WASM click handling is
    noticed rather than silently changing behaviour. Because of this, a test
    that opens a popup and then needs a canvas click must dismiss the popup
    first. Native replay isn't covered by Playwright; a C++ test of it is
    optional, since offscreen Qt may not replay clicks the way a desktop
    session does;
  - `ac` / `am` with no DB loaded, and loading a DB through the popup's
    "Load Database…" link;
  - Load/Update closes the popup before the chooser; it stays closed after
    success, cancellation or an error dialog. Reopening preserves search/sort
    on the same page and shows updated rows after a successful load;
  - search narrowing the table and the match count;
  - choosing a row and then drawing it, with the HELM checked;
  - the last-used label and its fade for a long symbol;
  - reactivation from the last-used button;
  - each page restoring its own DB monomer;
  - the reset rule, keyed on the page at close;
  - typing letters in the search box doesn't switch tools (see
    `popup_shortcuts.test.js`), and shortcuts work after the popup closes;
  - Ctrl+A in the search box selects the search text and leaves the canvas
    selection unchanged;
  - the popup stays within a small viewport when its minimum layout fits,
    including after shrinking the canvas while it is open; its table scrolls
    and its title, search/filter and close controls remain accessible;
  - below the minimum layout size, the popup's top-left stays reachable,
    with right/bottom overflow permitted;
  - mutation mode: browsing rows causes no mutation; click, Enter and Space
    each commit exactly once, keep the popup open and preserve the current
    row, which shows the highlight (screenshot of the table region);
  - Enter in tool mode doesn't close the popup;
  - removing the remembered DB monomer on an inactive page, and while
    atomistic mode is active, then returning to that page doesn't restore an
    empty DB tool;
  - CHEM chosen from both pages;
  - scrolling to an off-screen row in the ChEMBL DB and drawing it.
- **Update `monomer_database_regressions.test.js`.** It is built around
  `aa_unclassified_btn` and `na_unclassified_btn`, including the cases Step 2
  added or updated. Grep all of `test/wasm` for `unclassified`. Rewrite each
  case against the new popup:
  - the SKETCH-2870 palette-contents, scrolling, per-tab-memory and mutation
    tests. Per-tab memory is kept, now through the shared last-used button.
  - the `expectPalette(page, 'aa_unclassified_btn', …)` assertions in the
    SKETCH-2788 malformed-JSON test.
  - replace the `unclassified-*.png` references.
- **Regenerate** side-bar screenshot references that change because of the
  new row.

---

## Step 4: Filter popup and multi-select combo box

**Goal:**
- the filter button and filter popup from the mockup, without Matching
  SMILES or Apply;
- live filtering;
- a reusable multi-select combo box.

### 4a. `MultiSelectComboBox`

**Files:** `widget/multi_select_combo_box.{h,cpp}`, a `QComboBox` subclass.

**Source:** mmshare's `schrodinger/ui/qt/multi_combo_box.py` shows the
intent, but don't port its implementation:
- drop the Mac-specific workarounds (`MacMultiComboBoxStyle`, forcing a
  styled delegate, the `resetButton` hookup);
- drop the "All"/"None" pseudo-items and the af2 settings methods.

**Behaviour:**
- **Model:** use a `QStandardItemModel` with user-checkable items. Each item
  carries user data, so the popup can map it to `MonomerDbPolymerType`
  flags.
- **Toggling keeps the dropdown open:**
  - A mouse click on an item toggles it. Use an event filter on
    `view()->viewport()`: toggle when press and release land on the same
    enabled row, and swallow the release.
  - Space and Enter toggle too.
  - Escape and outside clicks close the dropdown.
  - Clear the view's hover highlight when the dropdown opens and closes.
- **Signal:** `checkedItemsChanged()` fires on any check change, including
  programmatic ones. Use `QSignalBlocker` for silent updates.
- **API:** `setCheckedData` / `checkedData`, `setItemChecked`, and
  `setMinimumCheckedCount(int)`. The filter popup uses 1, and unchecking the
  last checked item is ignored.
- **Display text:** paint the checked items joined with ", ", in item order.
  Show "Any" when every item is checked.
  - Use `QStylePainter` with the combo option's `currentText` overridden.
  - Never use `setCurrentText`.
- **Bold** works through the widget font, as for a normal combo.
- **Tests:** `test/schrodinger/sketcher/widget/test_multi_select_combo_box.cpp`.

### 4b. Filter popup

**Files:** `ui/monomer_database_filter_popup.ui` and
`widget/monomer_database_filter_popup.{h,cpp}`. It's a nested `Qt::Popup`,
positioned below the filter button with `show_popup_next_to_widget`.
Use the shared bounds rules, allowing placement above or clamping when needed.
Reposition an open filter popup if canvas resizing moves its anchor.

**Rows:**
- **Polymer type:** a `MultiSelectComboBox` with "Peptide", "Nucleic acid"
  and "Structure".
- **Monomer type:**
  - items: "Any", "Backbone", "Terminal";
  - enabled only when `isMonomerTypeApplicable()`.
- **Natural analog:**
  - items: "Any or None", "Any", "None", then `getNaturalAnalogChoices()`;
  - rebuilt whenever the polymer types change;
  - enabled only when `isNaturalAnalogApplicable()`.

**Footer:**
- "Clear All" (`filter_clear_all_link`) sets the most permissive filter:
  all three polymer types, monomer type Any, natural analog Any or None.
  It leaves search and sort unchanged and does not use the page default.
- "N filter(s) applied" (`filters_applied_lbl`, gray italics).
- An × close button (`filter_close_btn`) at top right.

**Behaviour:**
- **Live updates:** every change emits `filterChanged(MonomerDatabaseFilter)`
  immediately. The popup never touches the table.
- **Bold:** a combo is bold when its `is*Filtered()` is true, i.e. when it
  differs from the most permissive value.
- **Combos display the filter's effective values.** After every change,
  write each combo from the filter's effective getters, with signals
  blocked. So a disabled combo shows "Any" / "Any or None", and a re-enabled
  one shows the restored value. A user edit to an enabled combo calls the
  filter's setter. The combos never hold state of their own; the memory is
  in `MonomerDatabaseFilter` (see "Values hidden by applicability are
  remembered"). This also covers the analog combo, whose items are rebuilt
  when the polymer types change.

### 4c. DB popup integration

- **Filter button:** `monomer_db_filter_btn` sits next to the search box. It
  has a dropdown arrow and uses the `filter-menu_lb*` icons:
  - normal, hover and pressed for the ordinary states;
  - `_filtering` (orange) whenever `filter.isFiltering()`.

  It isn't user-toggleable; its look is derived from the filter. Clicking it
  opens the filter popup, initialised from the current filter.
  Reuse `configure_button_popup` so clicking the originating filter button
  dismisses its popup without reopening it, while other outside clicks replay
  on native builds (on WASM they only dismiss the popup).
- **`filterChanged`:** store the filter, re-filter, update the Type-column
  visibility and re-sync the selection.
- **Step 5 API:** add `setFilter(const MonomerDatabaseFilter&)`,
  `getFilter()` and `clearSearchText()`.

### Tests

- **C++ (filter popup and DB popup):**
  - the combo contents;
  - the multi-select default for each page;
  - enabling for every polymer-type combination:
    - Monomer type is enabled for {Peptide} and {Peptide, Structure} only;
    - Natural analog is enabled whenever exactly one of Peptide and Nucleic
      acid is checked;
    - both are enabled in the AA default, and only Natural analog in the NA
      default;
  - with {Peptide, Structure}, choosing Terminal or a specific analog hides
    the CHEM rows, and choosing None keeps them;
  - "Any" is displayed when all three types are checked;
  - the analog choices switch between peptide and nucleic acid;
  - a combo that becomes disabled shows "Any" / "Any or None", isn't bold,
    and doesn't count;
  - the Design-decisions example: {Peptide} + Backbone + C, then checking and
    unchecking Nucleic acid in the open dropdown, restores Backbone and C
    (enabled, bold, counted, table filtered);
  - checking Nucleic acid and then unchecking Peptide re-enables Natural
    analog at Any or None, and re-checking Peptide brings back neither C nor
    Backbone;
  - closing and reopening the filter popup while the combos are disabled,
    then restoring Peptide only, still restores Backbone and C;
  - Clear All and a page-default reset forget remembered values;
  - bold and count values;
  - Clear All selects all polymer types, Any and Any or None on either page;
    count becomes zero, bold styling clears and the button becomes normal,
    while search text and sort are preserved;
  - loading/updating through the DB popup closes it and any nested filter
    popup, then preserves the filter on normal same-page reopening;
  - every change re-filters the table live;
  - the button icon state, including orange in the default state and normal
    when all three types are checked;
  - the Type column hides for a single polymer type;
  - after enabling RNA on the AA page, choosing an RNA row switches to the
    NA page; closing and reopening there preserves the filter and search;
  - with a canvas selection under Select and Move/Rotate, merely navigating
    to that RNA row does not switch pages; committing does, while preserving
    the draw tool and remembered page tool.
- **Playwright:**
  - **Bridge:** add checkable-combo support.
  - **Extend `monomer_database_popup.test.js`:**
    - opening the filter popup;
    - checking and unchecking polymer types in the dropdown, with the
      dropdown, filter popup and DB popup all staying open (this triple
      popup nesting is the main WASM risk);
    - the last item can't be unchecked;
    - monomer type and analog enabling;
    - Backbone + C survive checking and unchecking Nucleic acid, while the
      disabled combos show "Any" / "Any or None"; checking Nucleic acid and
      then unchecking Peptide forgets both;
    - live table changes and the count label;
    - Clear All produces the most permissive filter and zero applied filters
      on either page, preserving search and sort;
    - Load/Update followed by normal same-page reopening preserves the filter
      as well as search/sort, with refreshed results;
    - after enabling RNA on the AA page, choosing an RNA row switches to the
      NA page while the popup stays open and anchored; closing and reopening
      on NA preserves the filter and search;
    - cross-page mutation with Select and Move/Rotate: navigation only
      browses, and click/Enter/Space commits and synchronizes the visible page;
    - clicking the filter button again dismisses without reopening;
    - the orange and normal button states (use the screenshot of the
      button);
    - the Type column visibility.
  - **Update any Step 3 tests** that assumed the filter can't change.

---

## Step 5: Core-only long-press popups and the folder entry

**Goal:**
- the long-press monomer popups have fixed core-ID membership, with displayed
  metadata resolved custom-first;
- each popup also has a folder entry that opens the DB popup, pre-filtered,
  but only when that filtered table would have at least one row (see "When
  the folder entry is shown");
- a button whose popup would be empty (no core analogs and no folder entry)
  has no popup, as today.

- **Core-only membership.**
  - Add a core-only query using the parameterised row-reading loop from
    Step 1a, for example `getCoreMonomersByNaturalAnalog(ChainType)`.
  - Use it to determine the fixed IDs and ordering in each palette. Core
    natural-analog associations determine which palette an ID belongs to;
    a custom override does not move it to another palette or add another entry.
  - Don't change `get_merged_na_analogs()`'s `ChainType::DNA` query. If
    core-only data makes touching it unavoidable, stop and tell the user.
- **Build the popups once, in the constructor.**
  - Build a popup widget for every AA button and every NA base, sugar and
    phosphate button, even one with zero core analogs, because a later
    database change can give it a folder entry. Do not rebuild popup widgets
    on database changes.
  - The exceptions are `unk_btn` (X) and `na_n_btn` (N), which stay
    popup-free. Their monomers are in the table.
  - Keep `MonomerToolWidget`'s `subscribeToChanges` subscription, but make
    it refresh the palettes in place: their displayed metadata, each folder
    entry's visibility and whether each button has a popup (see "When the
    folder entry is shown"). `SketcherModel` (Step 2) and the DB popup
    (Step 3) have their own subscriptions; don't move their work here, and
    don't make the palette refresh depend on either having run first.
  - Turn the `refresh_monomer_popups` test into "palette membership and widget
    identity stay fixed; displayed metadata and folder visibility follow the
    effective database".
- **A button whose popup would be empty has no popup.** A popup is empty
  when it has no core analogs (only its standard-symbol entry) and its
  folder entry is hidden. With only the core DB loaded, that's every NA
  base and the phosphate button, because each is its own only analog. The
  R and dR buttons keep their popups, since each lists the other.
  - Such a button behaves exactly like a button without a popup today: no
    popup indicator (including on hover), press-and-hold shows nothing, and
    clicking it while it's checked shows nothing.
  - This changes with the database, so it needs a non-destructive switch,
    for example `ToolButtonWithPopup::setPopupEnabled(bool)`. Disabling
    stops the press-and-hold timer, hides the popup if it's open, and turns
    off the indicators; enabling restores them. The popup widget itself is
    kept.
  - Remove `clear_popups` and `ToolButtonWithPopup::clearPopupWidget()` if
    nothing else uses them.
- **Custom-first displayed metadata.**
  - For every palette entry, including its standard-symbol entry, resolve its
    `MonomerID` through the existing custom-first `getMonomerInfo` lookup.
    Do this at initial construction too, since a custom DB may already exist.
  - Use the effective definition for names/tooltips and any other displayed
    metadata. If the custom override disappears, show core metadata again.
    Use the same refresh logic initially and after database changes; share it
    between AA and NA palettes rather than duplicating lookup/update logic.
  - Refresh the parent modular button's copied appearance/tooltip as well.
    Setting the same enum item is insufficient: `setEnumItem()` currently
    skips `updateButton()` when the ID is unchanged. Provide a presentation
    refresh path that does not click, emit activation, or change model state.
  - Preserve the palette's IDs, ordering, checked item, parent button's enum
    item and visibility during metadata refresh. Do not activate a tool or
    mutate a canvas selection. Folder entries retain their core-associated
    symbol/analog filter; they are not selectable monomer entries.
  - Activation continues to resolve the same ID custom-first, so the tooltip
    describes the definition that will actually be drawn. The searchable DB
    table independently keeps only the custom row for a shadowed core ID, as
    already specified by its source-list builder.
- **Folder entry** in `build_monomer_symbol_buttons()`
  (`widget/monomer_symbol_popup_utils.cpp`):
  - **Placement and look:** add a final row with a `QToolButton` named
    `analog_monomer_db_btn`. It shows `monomer_db.svg` with the popup's
    standard symbol drawn small in the upper-right corner.
    - Render the icon with a `QPainter` onto a pixmap, so the button stays
      a plain `QToolButton`.
  - **Not in the button group.** It must not join the popup's
    `QButtonGroup`, because it isn't a selectable analog and must not change
    the parent button's enum item.
  - **On click:**
    - it hides the symbol popup;
    - `AminoAcidSymbolPopup` / `NucleicAcidSymbolPopup` emit
      `monomerDatabaseRequested(MonomerDbPolymerType, NaturalAnalogFilter,
      std::string analog)`.
  - **Analog value:**
    - SPECIFIC with the button's symbol;
    - SPECIFIC `R` for dR.
- **When the folder entry is shown.** Show it only when its filter (below)
  matches at least one row of the DB popup's table source list, i.e. when
  clicking it would open a non-empty table. Otherwise hide it.
  - Compute this from
    `get_monomer_database_table_monomers(getCustomMonomers(),
    getCoreMonomers())` and `MonomerDatabaseFilter::matches()`, read once
    per database change for all buttons. Don't re-implement the inclusion or
    matching rules, and don't read the DB popup's cache (see
    "Database-change listeners are independent").
  - Build the filter with the same function that the click handler uses,
    so the visibility test and the opened table can't disagree.
  - Core monomers with an analog aren't in the table (they're in the
    palettes), and every core peptide and base is its own analog, so with
    only the core DB loaded no folder entry is shown. In practice the entry
    appears when a custom monomer has that polymer type and analog,
    including a custom override of a core ID such as `meG`.
  - Re-evaluate it on every database change, together with whether the
    button has a popup at all (see "A button whose popup would be empty has
    no popup"). If a database change hides the folder entry of an open
    popup, the popup stays open without it; if it leaves the popup empty,
    hide the popup.
  - dR and R share the filter SPECIFIC `R`, so they always agree.
- **`MonomerToolWidget` on `monomerDatabaseRequested`:**
  - build a filter with exactly that one polymer type, monomer type Any,
    and the given analog (the shared function above);
  - `setFilter` it and `clearSearchText()`;
  - open the popup from **`monomer_db_btn`**, not from the long-press popup;
  - skip the reset rule for this opening, so that it doesn't overwrite the
    requested filter.

  The page is recorded at close as usual, so the next ordinary opening on
  the same page keeps this filter.

  It doesn't activate anything.
- **Remove dead code.** Remove code paths that existed only for large
  custom-analog popups, such as the ">80 analogs" scroll area, if no core
  popup can reach that threshold.
- **C++ tests:**
  - palette IDs/order and popup widget identity stay unchanged after loading
    a custom DB; custom-only IDs do not enter the palettes;
  - an override of a core ID updates its palette tooltip and its parent
    button's copied tooltip when that entry is active, without changing the
    selected enum item, activating a tool or mutating the canvas;
  - metadata is custom-first when a custom DB is loaded before construction,
    and returns to core values when that override is removed;
  - overriding a core ID's natural analog does not move or duplicate the
    palette entry; activation resolves the effective custom definition;
  - the folder button exists, shows the right overlay and isn't in the
    group;
  - with only the core DB, no popup has a folder entry, every NA base
    button and the phosphate button have no popup (no indicator, and press-and-hold and a click while
    checked show nothing), and peptide buttons keep their core-analog
    popups;
  - loading a custom monomer with polymer type PEPTIDE and analog A shows
    the folder entry for Ala only; loading one with RNA and analog A gives
    the adenine button a popup with its standard entry and the folder
    entry; resetting the database removes both again, without changing
    popup widget identity;
  - a custom monomer matching only in another polymer type (e.g. RNA `A`)
    doesn't show Ala's folder entry, and dR and R show theirs together;
  - a database change that empties an open popup hides it;
  - clicking it opens the DB popup anchored to `monomer_db_btn`, with the
    expected filter and an empty search;
  - update `test_nucleic_acid_symbol_popup.cpp` and
    `test_modular_tool_button.cpp`.
- **Playwright.**
  - **New cases:**
    - the folder entry from a peptide button, a base and dR (filtered to
      `R`);
    - each opens the DB popup at the side-bar button, with the right
      filter, count and description, and a cleared search;
    - a normal reopening afterwards keeps that filter;
    - with no custom DB, Ala's popup has no folder entry and the adenine
      button has no popup; after loading a DB with a peptide A analog and an
      RNA A analog, both have folder entries, and the adenine popup opens;
      loading a DB without them removes them again;
    - a custom override of core PEPTIDE `meG` updates the G palette's tooltip
      and the parent button's tooltip without changing palette membership;
      selecting it draws the custom definition, and removing the override
      restores core metadata;
    - the searchable DB popup shows only the custom entry for that shadowed
      ID, with no duplicate core row.
  - **Update tests that pick custom monomers from long-press popups.** These
    will break. Switch them to `chooseFromMonomerDatabase` (or the folder
    entry), and grep `test/wasm/e2e` for `choosePopup`, `clickPopupTool`,
    `popupButtons` and `popupInfo` used after `loadDatabase` to find any
    others. Known cases:
    - **`monomer_database_regressions.test.js`, SKETCH-2788:**
      - `gly_btn` → `Boc_G` after loading ChEMBL;
      - the `['G', 'meG']` palette after loading native definitions.
    - **`tst_long_monomer_label.test.js`:**
      - `ser_btn` → the long ChEMBL symbol;
      - SKETCH-2871's `ala_btn` → `LongAla`.
    - `analog.test.js` (`analog_dA_btn`) uses a core monomer and should be
      unaffected; verify it.

---

## Step 6: Description and structure image

**Goal:** the right-hand pane of the DB popup.

- **Description label** (`monomer_db_description_lbl`, gray italics), to the
  right of the filter button. It shows `filter.getDescription()` and updates
  live with the filter.
- **Image** (`monomer_db_image_lbl`, in a rounded frame):
  - **Sizing:** scale the preview within its available pane, preserving
    aspect ratio. Its natural image size must not force the popup beyond the
    available bounds; coordinate with table-viewport shrinking under the
    shared bounds policy. Recalculate on available-area changes while open.
  - **Preview row:** the selected row, or the valid current index if there
    is no selected row. Update on both selection and current-index changes,
    and explicitly after signal-blocked synchronization or model resets.
    Keep the current-row preview visible after a mutation commit.
  - **What to render:** the preview row's `MonomerInfo::smiles`, **as
    stored**. Capping groups and atom-map numbers stay visible, with no
    capping or conversion.
  - **How:**
    - Parse it the way `get_tool_tip_containing_smiles_image()` in
      `molviewer/abstract_monomer_item.cpp` does
      (`rdkit_extensions::to_rdkit(…, EXTENDED_SMILES)` + `prepare_mol`).
    - Extract a shared helper rather than duplicating it.
    - Render with `image_generation.h` (`get_qimage` or `get_qpicture`).
    - Check that atom-map numbers display. If the renderer's options hide
      them, enable them for this image only.
- **Empty or failed state:** when neither a selected row nor a valid current
  index exists, or the SMILES fails to parse, show an empty frame. Never
  throw from a selection or current-index change.
- **Performance:**
  - Cache the image for the current `MonomerID`.
  - Invalidate the image cache on every database refresh, even when the ID
    survives: the same symbol and polymer type may now have different SMILES.
    Clear the old image and re-render the resolved preview row from the new
    cache. A failed replacement render must leave an empty frame, not the
    old structure. Any pending deferred render must use the latest database
    contents and preview row, not retained indices or references from a reset.
  - Keyboard-scrolling through 12k rows must stay responsive. If needed,
    defer rendering with a 0 ms single-shot timer that coalesces rapid
    changes.
- **C++ tests:**
  - the description for each filter state;
  - the image follows selection, or current index when selection is empty;
  - keyboard browsing with a canvas selection updates the preview without
    mutation, and click/Enter/Space commits leave that preview visible;
  - clearing selection preserves a valid current-row preview; losing both
    selection and current index clears it;
  - a same-ID/different-SMILES database replacement refreshes the image,
    including when the replacement fails to render;
  - an unparseable SMILES not crashing.
  - preview scaling and table shrinking preserve the bounds policy after
    adding the right-hand pane, including top-left precedence below the
    minimum layout size.
- **Playwright:**
  - the description text through several filter changes;
  - the image appearing for a selected row or an unselected current row,
    persisting after a mutation commit, and clearing when neither exists
    (screenshot reference of the image region);
  - replacing a database with changed SMILES under the same ID refreshes
    the preview; a failed render clears the old image;
  - fast keyboard scrolling through the ChEMBL DB staying responsive.
  - repeat the small-viewport and live canvas-resize checks with the preview
    pane present: the image scales without distortion and the popup remains
    within bounds whenever its minimum layout fits.
  - Update earlier suites' popup screenshots, which now include the
    right-hand pane.
