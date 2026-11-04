# Monomer Database popup: detailed implementation plan (SKETCH-2827)

This expands `monomer db popup initial plan.md` into one self-contained brief per
step. Each step is sized for one sub-agent and ends in a reviewable,
building, tested state. Items marked **[Qn]** depend on an open question in
the "Open questions" section at the bottom. Until you answer, the plan follows
the stated default.

## Changes from the initial plan

1. **The "Unclassified" buttons are removed in Step 2, not Step 3.** The new
   last-used button takes over exactly the role the Unclassified buttons play
   today: `DrawTool::MONOMER_DB_MONOMER`, `ModelKey::MONOMER_DB_MONOMER`, and
   the `AminoAcidTool::UNCLASSIFIED`/`NucleicAcidTool::UNCLASSIFIED` "the page's
   active tool is a DB monomer" marker. Both buttons can't map to the same enum
   value in `m_button_amino_acid_bimap`/`m_button_nucleic_acid_bimap` (a bimap
   needs unique right-hand values). Keeping both for one step would mean adding
   throwaway enum values and branches. Step 2 also renames `UNCLASSIFIED` to
   `MONOMER_DATABASE`.
2. **The old Step 2 is split** into Step 2 (side-bar buttons, DB popup, search,
   table, tool sync) and Step 3 (filter popup). Each half is still a substantial
   change, and splitting them makes them easier to review.
3. **The DB is read once per DB change, and all filtering happens in memory.**
   `MonomerDatabase` gets one public `getAllMonomers()` call that applies the
   same custom-overrides-core rule as the single-monomer lookups. Filtering by
   type and analog, text search, and sorting are pure functions over
   `std::vector<MonomerInfo>`. Changing filters needs no SQL, the logic tests
   without the DB singleton, and the database layer needs only one new query.
   Even 12k monomers (`chembl_monomers_x5.json`) is cheap to filter in memory.
4. **`QAbstractTableModel`** instead of a bare `QAbstractItemModel` subclass. It's
   the standard base class for a flat table.
5. **Shared helpers are extracted instead of duplicated** (per your style rule):
   - "Which `MonomerID` is the active tool drawing?" Extracted from
     `get_monomer_symbol_button_id()` in `widget/monomer_symbol_popup_utils.cpp`.
   - "Position a popup below or above a button, clamped to the screen." Extracted from
     `ToolButtonWithPopup::showPopup()`, which is protected and button-specific.
   - "Paint text with a fade-out" extracted from `AbstractMonomerItem::paint()`
     and `elide_text()` in `molviewer/abstract_monomer_item.cpp`.

## Shared context for every step

Every sub-agent should read this section first.

- Read the repo `AGENTS.md`/`CLAUDE.md` (building, formatting, staging new files,
  no commits). Use the `sketcher-architecture`, `sketcher-monomer-helm`, and
  `sketcher-build-and-debug` skills. Steps 2–5 should also use
  `implement-ux-spec` and `schrodinger-cpp-style`.
- The spec is `F:\Documents\sketcher\SKETCH-2827\Dual Mode Sketcher v1 - design
  updates.docx`, section "Using the Monomer DB" → "Longer term approach". The
  relevant mockups are `image-20260727-212310.png` (main popup) and
  `image-20260728-133216.png` (filter popup) in the same folder. Where this plan
  or the initial plan differs from the spec, the plan wins.
- Out of scope: the "Matching SMILES" filter, the "Recently used items" palette,
  and dR in the nucleic-acid analog list.
- **Monomer DB facts**
  (`include/schrodinger/rdkit_extensions/monomer_database.h`):
  - There's one singleton, `MonomerDatabase::instance()`. It always holds the
    core DB (`m_core_monomers_db`, generated from
    `src/schrodinger/rdkit_extensions/default_monomer_definitions.json`: 70
    monomers, of which 60 are PEPTIDE Backbone, 2 are PEPTIDE Terminal with
    analog `X` (`am` and `ac`), and 8 are RNA; it has **no CHEM monomers**). It
    can also hold a custom DB.
  - Single lookups (`get_info_from_database`) prefer custom over core.
    `getMonomersByPolymerType()` returns **both** with no de-duplication, so a
    custom monomer that shadows a core symbol would appear twice.
  - `MONOMER_TYPE` values seen in the data are `Backbone`, `Branch`, and
    `Terminal`. Tests use lowercase (`"backbone"`), so compare
    case-insensitively.
  - `subscribeToChanges()` returns a `scoped_connection`. The existing widget
    code marshals the callback with `QMetaObject::invokeMethod(this, …,
    Qt::AutoConnection)`. Follow that pattern.
  - DNA monomers use `ChainType::RNA`. Don't add `ChainType::DNA` branches or
    tests.
- **Natural analog "None"** means an empty string **or** the polymer type's
  unknown residue: `X` for PEPTIDE and `N` for RNA. Current DBs use `X`/`N`, and
  SHARED-12734 will switch them to empty. **`N` is asparagine for peptides and
  `X` means nothing special for RNA**, so the "None" check must depend on the
  polymer type.
- **Tool state** lives in `SketcherModel` (`model/sketcher_model.h`), not in
  `SketcherWidget`. A DB monomer is active when `getDrawTool() ==
  DrawTool::MONOMER_DB_MONOMER`, `ModelKey::MONOMER_DB_MONOMER` holds an
  `rdkit_extensions::MonomerID`, and `MONOMER_TOOL_TYPE` says which page it
  belongs to. `Scene::getNewSceneTool()` builds the `DrawMonomerSceneTool`
  from those values. If there's an active canvas selection, widgets **ping**
  the key instead of setting it, and `SketcherWidget` then mutates the selected
  monomers (`sketcher_widget.cpp`, `case ModelKey::MONOMER_DB_MONOMER`).
- Tests are auto-discovered (one executable per `test/**/*.cpp`). Tests that
  load a DB must reset it on exit. Copy the `ResetDatabase` RAII struct pattern
  from `test/schrodinger/sketcher/widget/test_monomer_tool_widget.cpp`.
- When you finish: run clang-format on the changed C++, stage new, deleted, and
  renamed files (ask the user, because the sandbox blocks `.git/index.lock`),
  and don't commit.

---

## Step 1: Filtering logic and table model

**Goal:** All of the popup's non-visual logic, unit tested, with no UI yet.

### 1a. `MonomerDatabase::getAllMonomers()` (rdkit_extensions)

- Add `[[nodiscard]] std::vector<MonomerInfo> getAllMonomers() const;` to
  `MonomerDatabase`. It returns every monomer from both DBs, de-duplicated on
  `(symbol, polymer type)` with **custom winning over core**, which matches
  `get_info_from_database`.
- Refactor the private `getMonomers(ChainType, bool)` so that both functions
  share the prepare/step/`assign_monomer_info` row-reading loop. Don't copy it.
- Tests in `test/schrodinger/rdkit_extensions/test_monomer_database.cpp` cover:
  core-only contents, custom additions, a custom monomer shadowing a core one
  (it appears once, with the custom fields), and `resetMonomerDefinitions()`.

### 1b. `MonomerDatabaseFilter` and the pure functions (Qt-free) **[Q14]**

New files: `src/schrodinger/sketcher/model/monomer_database_filter.{h,cpp}`.
These are Sketcher-side because they contain UI strings and Sketcher filter
semantics. They must include no Qt headers. Proposed API (the sub-agent may
refine names):

```cpp
enum class MonomerDbPolymerType : unsigned {
    NONE = 0, PEPTIDE = 1 << 0, NUCLEIC_ACID = 1 << 1, CHEM = 1 << 2,
};
// operator| / operator& / has_flag helpers

enum class MonomerTypeFilter { ANY, BACKBONE, TERMINAL };
enum class NaturalAnalogFilter { ANY_OR_NONE, ANY, NONE, SPECIFIC };
enum class MonomerDbSortField { SYMBOL, NAME, NATURAL_ANALOG };

class MonomerDatabaseFilter {
  public:
    // the set of polymer types this popup can offer (page-dependent):
    // PEPTIDE|CHEM for the amino page, NUCLEIC_ACID|CHEM for the nucleic page
    explicit MonomerDatabaseFilter(MonomerDbPolymerType available_types);

    // setters/getters for polymer types, monomer type, analog mode + value,
    // sort field + direction
    bool isMonomerTypeApplicable() const;   // polymer types == PEPTIDE
    bool isNaturalAnalogApplicable() const; // == PEPTIDE or == NUCLEIC_ACID
    std::vector<std::string> getNaturalAnalogChoices() const; // [Q-list]
    void clear();                           // "Clear All" state [Q6]
    int getNumFiltersApplied() const;       // [Q7]
    std::string getDescription() const;     // [Q8]
    bool matches(const rdkit_extensions::MonomerInfo&) const;
    bool operator==(const MonomerDatabaseFilter&) const = default;
};

std::vector<MonomerInfo> filter_and_sort_monomers(
    const std::vector<MonomerInfo>& all_monomers, const MonomerDatabaseFilter&);
std::vector<MonomerInfo> filter_monomers_by_search_text(
    const std::vector<MonomerInfo>& monomers, std::string_view text);
```

Semantics:
- **Polymer type:** map `MonomerInfo::polymer_type` with
  `rdkit_extensions::toChainType`. PEPTIDE→PEPTIDE, RNA→NUCLEIC_ACID,
  CHEM→CHEM. Drop rows with a missing or unparseable type.
- **Applicability rules are the single source of truth.** When
  `isMonomerTypeApplicable()` is false, the monomer-type value is ignored by
  `matches()`, by the count, and by the description. The same goes for the
  analog. Step 3 calls the same methods to enable and disable the combo boxes.
  When a filter becomes inapplicable, reset it to ANY or ANY_OR_NONE so that a
  disabled combo never hides a stale value.
- **Monomer type:** case-insensitive comparison with `Backbone`/`Terminal`.
- **Natural analog:** ANY_OR_NONE matches everything. NONE matches an empty
  value or the type-specific unknown (X or N). ANY is the complement of NONE.
  SPECIFIC is an exact match. **Don't hard-code `"X"`/`"N"`**: find the existing
  constants (`AMINO_ACID_TOOL_TO_RES_NAME.at(UNK)`, the NA `N` entry, or an
  rdkit_extensions constant) and reuse them.
- **Analog choices:** peptide uses the 20 standard one-letter codes from
  `AMINO_ACID_TOOL_DISPLAY_ORDER` minus UNK. Nucleic acid uses
  `R, P, A, C, G, T, U` (no dR). Spec discrepancy: the spec's peptide list
  includes `X` and omits `N`. This plan includes `N` and leaves `X` out (`X` is
  covered by "None").
- **Sort:** case-insensitive, with deterministic tie-breaks (then symbol, then
  polymer type) and a stable sort. Missing values sort last in ascending order.
- **Search:** case-insensitive substring on symbol **or** name, preserving the
  input order. Empty text returns the input unchanged. The spec's "hydro" example
  matches both "Hydroxyisoleucine" and "Me_Phe(a,b-dehydro)", so this is a
  substring search, not a prefix search.

Tests: `test/schrodinger/sketcher/model/test_monomer_database_filter.cpp`. Use
hand-built `MonomerInfo` vectors so no DB is needed. Cover every
applicability combination, None/Any with `X`, `N`, and empty for both types
(including peptide `N` = Asn **not** counting as None), case-insensitive
monomer type, sort fields and directions with ties, search on symbol vs name,
the count and description for each mockup state, and `clear()`.

### 1c. `MonomerDatabaseTableModel` (Qt)

New files: `src/schrodinger/sketcher/widget/monomer_database_table_model.{h,cpp}`,
a `QAbstractTableModel` subclass.
- `setMonomers(std::vector<MonomerInfo>)` uses `beginResetModel`/`endResetModel`.
- Columns: Symbol, Name, Analog **[Q10]**. Use `Qt::DisplayRole` with a
  centered analog. Header data comes from `headerData`.
- `sort(int column, Qt::SortOrder)` override: emit a
  `sortRequested(MonomerDbSortField, bool ascending)` signal (or take a
  callback) so that the popup updates its `MonomerDatabaseFilter` and
  re-runs the pure functions. The model doesn't sort itself, which keeps the
  business logic in 1b.
- Helpers: `MonomerID getMonomerID(int row) const`,
  `std::optional<int> getRow(const MonomerID&) const`, and
  `const MonomerInfo& getMonomerInfo(int row) const`.
- Tests: `test/schrodinger/sketcher/widget/test_monomer_database_table_model.cpp`,
  covering row and column counts, display data, header labels, reset, row↔ID
  lookups, and the sort signal.

**Done when:** the new tests and `test_monomer_database` pass, and nothing in
the UI has changed.

---

## Step 2: Side-bar buttons and the Monomer Database popup (no filter popup yet)

**Goal:** The two new side-bar button pairs replace the Unclassified buttons.
Clicking a DB button opens a popup with search, a table, a match count, and
Close. Selecting a row activates the monomer. The filter is fixed to the page
default **[Q6]**. There's no filter button, description, or image yet.

### 2a. Remove the Unclassified buttons and rename the enum values

- Rename `AminoAcidTool::UNCLASSIFIED` and `NucleicAcidTool::UNCLASSIFIED` to
  `MONOMER_DATABASE` everywhere: `sketcher_model.h`, `scene.cpp`,
  `sketcher_widget.cpp`, `sketcher_side_bar.cpp`, `monomer_tool_widget.cpp`,
  and the tests.
- Delete `aa_unclassified_btn`/`na_unclassified_btn` from
  `ui/monomer_tool_widget.ui`. In `MonomerToolWidget`, delete the
  unclassified-specific code: the X/N + CHEM popup construction, the
  `-1` enum-item handling, `getMonomerDbMonomerForButton`, and the
  unclassified branch of `updateCheckedButton`. Leave the normal analog-popup
  construction alone. Step 4 replaces it.
- Remove or rewrite the tests that exercise the Unclassified buttons:
  `unclassified_monomers` and `unclassified_chem_monomers` in
  `test_monomer_tool_widget.cpp`, and the related cases in
  `test_sketcher_widget.cpp` and `test_sketcher_model.cpp`. Their scenarios
  (CHEM reachable from both pages, mutation by ping, page switching restoring
  the DB tool) carry over to the new buttons in 2d.

### 2b. Shared helpers (extract, then reuse)

- `std::optional<rdkit_extensions::MonomerID> get_active_monomer_id(const
  SketcherModel*)`: return the monomer that the active draw tool would place,
  using `MONOMER_DB_MONOMER` for the DB tool, `AMINO_ACID_SYMBOL` + PEPTIDE or
  `NUCLEIC_ACID_SYMBOL` + RNA for `DrawTool::MONOMER`, and nullopt for full
  nucleotides and other tools. Rewrite `get_monomer_symbol_button_id()` to use
  it. **[Q2]**
- `void show_popup_next_to_widget(QWidget* popup, QWidget* anchor)`
  (`widget_utils`): the positioning logic from
  `ToolButtonWithPopup::showPopup()`, which must then call it.
- `paint_text_with_fade_out(...)` and the existing `elide_text()`: move the
  gradient and fade painting out of `AbstractMonomerItem::paint()` into a
  reusable function. `AbstractMonomerItem` calls it with its border path as the
  clip. The new button calls it with its contents rect. Keep the existing
  monomer-item rendering tests green.

### 2c. New widgets

- **Icons:** add `icons/monomer_db.svg` to `src/schrodinger/sketcher/sketcher.qrc`.
  Copy `filter-menu_lb.png`, `filter-menu_lb_act.png`,
  `filter-menu_lb_filtering.png`, and `filter-menu_lb_hvr.png` from
  `F:\repos\src\mmshare\python\modules\schrodinger\ui\qt\standard\icons\icon_data\filter-menu`
  into `src/schrodinger/sketcher/icons/` and add them to the qrc now so that
  Step 3 can use them.
- **Last-used button** (`widget/monomer_database_last_used_button.{h,cpp}`, a
  `QToolButton` subclass): checkable. It holds an
  `std::optional<MonomerID>`, shows the symbol through
  `paint_text_with_fade_out` when the symbol doesn't fit the button width,
  and uses the monomer name as a tooltip. It's blank and unchecked when
  empty **[Q11]**. Style it with `ATOM_ELEMENT_OR_MONOMER_STYLE` or the closest
  existing style.
- **DB popup** (`ui/monomer_database_popup.ui` +
  `widget/monomer_database_popup.{h,cpp}`, a `SketcherViewWithWasmOutlineFix`
  with `Qt::Popup`, modeled on `FileSaveImagePopup`): title "Monomer Database"
  with an × close button, a search `QLineEdit` (search icon and clear button;
  the clear button is `setClearButtonEnabled`), a `QTableView` (single row
  selection, select rows, sorting enabled, no editing, no vertical header,
  stretch the Name column), an "N matches" label, and a "Close" text link
  (`TEXT_LINK_STYLE`). Leave room on the right for Step 5's description and
  image. When nothing matches, show "No monomers match search & filter
  criteria" in gray italics in place of the table. **[Q9]** decides whether
  "Load Database…" or "Update Database…" appear.
  - The constructor takes `MonomerToolType page`, which sets the filter's
    available types (PEPTIDE|CHEM or NUCLEIC_ACID|CHEM) and the default filter.
  - It caches `getAllMonomers()` and refreshes the cache on
    `subscribeToChanges`. The filtered list is recomputed only when the filter
    or sort changes. The text search re-runs on `textChanged`. After any
    contents change, re-sync the selection (a model reset clears it).
  - Signal `monomerSelected(MonomerID)` fires on user row selection, including
    keyboard navigation. Method `syncSelectionToModel()` selects the row for
    `get_active_monomer_id(model)` (only when the ID's polymer type is in the
    table) or clears the selection. Use `QSignalBlocker` on the selection
    model to break the selection → model → selection loop. Selecting a row
    doesn't close the popup.
  - Filter, search text, and sort persist across openings, because each page
    has one popup instance.
- **Layout:** in `amino_page` and `nucleic_page`, add a row where the
  Unclassified button was. The last-used button goes in column 0 and a DB
  `QToolButton` with the folder icon and a popup-indicator arrow goes in
  columns 1–2. Match the spec's "NMA [folder▾]" look.

### 2d. Wiring in `MonomerToolWidget`

- Add the last-used buttons to `amino_monomer_group`/`nucleic_monomer_group`
  and map them to `MONOMER_DATABASE` in the bimaps. This keeps exclusive
  checking and `getCheckableButtons` working.
- **DB button click:** if this page has a last-used monomer, activate it first
  (the same way press-and-hold activates a popup's button), then show the popup
  with `show_popup_next_to_widget`. **[Q2]**
- **Last-used button click:** activate its monomer. Reuse the existing
  `activateMonomerDbMonomer` logic: ping if there's a selection, otherwise set
  `DRAW_TOOL`, `MONOMER_DB_MONOMER`, `MONOMER_TOOL_TYPE`, and
  `AMINO_ACID_TOOL`/`NUCLEIC_ACID_TOOL = MONOMER_DATABASE`. Change it to take a
  `MonomerID` and a page instead of a button. **[Q11]**
- **`popup.monomerSelected(id)`:** store `id` as the page's last-used monomer
  and activate it **[Q1][Q3]**.
- **`updateCheckedButton()`:** when `draw_tool == MONOMER_DB_MONOMER`, there's
  no selection, and the page matches, check this page's last-used button and
  set its monomer from `MONOMER_DB_MONOMER`. This covers activation from
  anywhere, including WASM or API calls. Always call
  `popup->syncSelectionToModel()`. `onAminoOrNucleicBtnClicked` restores the
  page's last-used DB monomer as it does today, keyed on `MONOMER_DATABASE`.
- **DB reload:** if a last-used monomer no longer exists, clear that button.
- **Enabled state:** follow `updateWidgetsEnabled()`. The AA pair is enabled
  like the AA buttons. The NA pair is enabled like the NA base buttons, or
  always when there's no monomer selection. Confirm against the mutation rules.

### Tests

Add to `test_monomer_tool_widget.cpp`, or a new
`test_monomer_database_popup.cpp` for popup-only behavior:
- The popup shows the right polymer types per page, including CHEM from both
  pages.
- Search narrows the rows, and the match count is correct.
- Selecting a row sets the model and checks and labels the last-used button
  without closing the popup.
- A model change from elsewhere moves the table selection and clears it when
  the tool isn't in the table.
- Opening the DB button with a last-used monomer activates that monomer.
- Page switching restores each page's own last-used monomer.
- Pinging with a selection mutates it **[Q3]**.
- A DB reload refreshes the table, and a monomer that disappeared clears the
  last-used button.
- A long symbol triggers the fade path.

Also add a `SketcherSideBar` atomistic↔monomeric toggle case.

**Manual (user):** verify on WASM that typing in the search box doesn't trigger
keyboard shortcuts, that shortcuts work after the popup closes (compare
`test/wasm/e2e/popup_shortcuts.test.js`), and that the popup stays inside a
small canvas (SKETCH-2920).

---

## Step 3: Filter popup

**Goal:** the filter button and popup from the mockups.

- `ui/monomer_database_filter_popup.ui` +
  `widget/monomer_database_filter_popup.{h,cpp}`: a nested `Qt::Popup` opened
  from the DB popup's filter button and positioned with
  `show_popup_next_to_widget`.
  - **Polymer type combo:** for the AA page, "Peptide or structure", "Peptide",
    "Structure". For the NA page, "Nucleic acid or structure", "Nucleic acid",
    "Structure". The "X or structure" entry replaces "Any". Note: the initial
    plan wrote "Nucleic Acid". UX sentence case gives "Nucleic acid", which this
    plan uses.
  - **Monomer type combo:** "Any", "Backbone", "Terminal". It's enabled only when
    `isMonomerTypeApplicable()`.
  - **Natural analog combo:** "Any or None", "Any", "None", then
    `getNaturalAnalogChoices()`. It's enabled only when
    `isNaturalAnalogApplicable()`. The choices are rebuilt when the polymer type
    changes.
  - Non-default values are shown in **bold** (per the spec). There's a "Clear
    All" link, an "N filter(s) applied" label, an "Apply" button, and an ×
    button. **[Q5][Q6][Q7]**
  - The popup edits a **copy** of `MonomerDatabaseFilter` and emits
    `filterApplied(MonomerDatabaseFilter)`. It never touches the table directly.
- **Filter button** on the DB popup: use the four `filter-menu_lb*` icons for
  normal, hover, pressed, and filters-applied, choosing the state from
  `getNumFiltersApplied() > 0`.
- Wire `filterApplied` → update the stored filter → re-filter → re-sync the
  selection.
- Add `void setFilter(const MonomerDatabaseFilter&)` and
  `void clearSearchText()` to the DB popup (Step 4 needs them).
- **Tests:** combo contents per page, enabling and disabling as the polymer
  type changes, a disabled combo resetting to Any, the count label, Clear All,
  Apply vs. dismiss **[Q5]**, the icon state, and an applied filter changing
  the table rows.

---

## Step 4: Simplify the monomer-button popups and add the folder entry

**Goal:** the long-press popups show only core monomers plus a folder button
that opens the DB popup pre-filtered.

- **Core-only data:** add a way to query only `m_core_monomers_db`. One option
  is a `getMonomersByNaturalAnalog(ChainType, bool core_only = false)`
  parameter on the same shared private loop from Step 1. Use it in
  `MonomerToolWidget::updateMonomerButtons()` and in
  `get_merged_na_analogs()`.
  - **Also fix:** `get_merged_na_analogs()` (`rdkit/monomer_analog.cpp`)
    queries `ChainType::DNA`, which violates the "DNA is RNA" invariant. With
    core-only RNA data, the merge reduces to a single RNA query, so simplify
    it.
- **Build the popups once** in the constructor. Remove the
  `subscribeToChanges` subscription, `clear_popups`, and
  `ToolButtonWithPopup::clearPopupWidget()` if nothing else uses it. Remove the
  `refresh_monomer_popups` test or turn it into "popups don't change on DB
  load". **[Q12]**: every AA button (except X) and every NA base, sugar, and
  phosphate button (except N) now gets a popup, even with zero analogs,
  because the folder entry always exists.
- **Folder entry** in `build_monomer_symbol_buttons()`
  (`widget/monomer_symbol_popup_utils.cpp`): add a final row with a
  `QToolButton` showing `monomer_db.svg` with the popup's standard symbol drawn
  small in the upper-right corner. Render it with a `QPainter` onto a pixmap
  for the icon, so the button stays a plain `QToolButton`. **The folder button
  must not join the popup's `QButtonGroup`**, because it isn't a selectable
  analog and must not change the parent button's enum item. Clicking it hides
  the symbol popup and emits `monomerDatabaseRequested(polymer type, natural
  analog)` from `AminoAcidSymbolPopup`/`NucleicAcidSymbolPopup`. For dR, the
  analog is `R`.
- **`MonomerToolWidget`:** on `monomerDatabaseRequested`, build a cleared
  filter, set the polymer type to Peptide or Nucleic acid and the analog to
  SPECIFIC(symbol), apply it with `setFilter`, clear the search text
  **[Q12]**, and open the page's DB popup from the **DB side-bar button**
  (reuse the 2d click path, including last-used activation **[Q2]**).
- Remove the code paths that existed only for non-core analogs, such as the
  "> 80 analogs" scroll area, if nothing still needs them. Keep them if a core
  popup could still exceed the threshold.
- **Tests:** popup contents equal core analogs only and don't change after
  loading a custom DB. The folder button exists with the right overlay text,
  isn't in the button group, and clicking it opens the DB popup anchored to the
  DB button with the expected filter and an empty search. Update
  `test_nucleic_acid_symbol_popup.cpp` and `test_modular_tool_button.cpp` as
  needed.

---

## Step 5: Atomistic image and filter description

**Goal:** the right-hand pane of the DB popup.

- **Description label** (gray italics) above the image: shows
  `filter.getDescription()` and updates whenever the filter changes.
- **Image:** when the table selection changes, render the selected monomer's
  `MonomerInfo::smiles` with `image_generation.h` (`get_qimage` or
  `get_qpicture` from an RDKit mol). Parse it the way
  `get_tool_tip_containing_smiles_image()` in `abstract_monomer_item.cpp`
  does (`rdkit_extensions::to_rdkit(…, EXTENDED_SMILES)` + `prepare_mol`). If
  a shared helper is needed, extract one rather than duplicating it.
  **[Q13]** decides whether to show capped atoms (OH/NH2, as in the mockup)
  or attachment-point labels. DB SMILES encode leaving groups as atom-mapped
  atoms, for example `N[H:1]` and `C(=O)[OH:2]`, so look for an existing
  monomer→atomistic or capping helper in `rdkit_extensions` before writing
  one.
- When nothing is selected or the SMILES fails to parse, show an empty
  rounded frame. Never throw from a selection change.
- Cache the image for the current `MonomerID`. Keyboard-scrolling through 12k
  rows must stay responsive. If needed, defer rendering with a 0-ms
  single-shot timer that coalesces rapid changes.
- **Tests:** the description for each filter state, the image updating on
  selection and clearing on deselection, and an unparseable SMILES not
  crashing.

---

## Open questions (the plan uses the default until you answer)

| # | Question | Default |
|---|---|---|
| Q1 | When a row has its own side-bar button (e.g. PEPTIDE `A` or `dA`), does selecting it use the DB tool and last-used slot, or switch to the side-bar button? | DB tool and last-used slot |
| Q2 | Does table selection follow *any* active monomer tool (choosing Ala on the side bar selects the `A` row), or only the DB tool? And does opening the popup activate the page's last-used monomer (the spec's "opening the popup automatically loads the last-used item")? | Any monomer tool; yes, opening activates the last-used monomer |
| Q3 | With a canvas selection, does clicking a row mutate the selection (ping), as the Unclassified buttons do now? | Yes |
| Q4 | When a click outside the popup dismisses it, should that click also reach the canvas? The spec notes this "may accidentally add item". | Swallow it (`WA_NoMouseReplay`) |
| Q5 | Does the filter popup keep **Apply** (changes pending until Apply; × discards), or apply live? | Keep Apply |
| Q6 | What is the default filter when a popup is first opened, and what does Clear All reset to? The spec says PEPTIDE/backbone by default, Clear All = Any. | Default: Peptide (or Nucleic acid) + Any + Any or None. Clear All: "… or structure" + Any + Any or None |
| Q7 | How are "filters applied" counted? The mockups disagree (Peptide+Backbone = 1, Peptide+Terminal = 2, Peptide+Backbone+C = 2). | Count each applicable combo that differs from its Clear-All value |
| Q8 | What is the description text format? | `Peptide monomers`, `Peptide, terminal monomers`, `Peptide, Cys analogs` (three-letter code), `Nucleic acid, A analogs`, `Peptide, unclassified monomers` (None), `Structure monomers`, `Peptide or structure monomers` |
| Q9 | Should the popup include "Load Database…"/"Update Database…" and the "No monomer database found" state? The core DB always exists, and `main.cpp` hides the import action with `setLoadMonomerDatabaseVisible(false)`. | Omit for now |
| Q10 | Should a "Type" column be added when CHEM is mixed with peptide/NA rows (the same symbol can exist in both)? Should "None" analogs (`X`/`N`/empty) display blank? | No Type column; blank for None |
| Q11 | What happens when the user clicks the blank (never-used) last-used button? | Opens the DB popup |
| Q12 | Step 4: does every AA/NA button (except X/N) get a popup now? Does the folder entry also clear the search text? | Yes and yes |
| Q13 | Step 5: show the capped structure, as in the mockup, or attachment points? | Capped, as in the mockup |
| Q14 | Where should the Qt-free logic live: `sketcher/model/`, or a new `sketcher/monomer_db/` folder? | `sketcher/model/` |
