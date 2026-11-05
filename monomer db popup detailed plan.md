# Monomer Database popup: implementation plan (SKETCH-2827)

This is the single source of truth for SKETCH-2827. It replaces
`monomer db popup initial plan.md`. The work is split into five steps. Each step
is one self-contained brief for a sub-agent, and each ends in a building,
tested, reviewable state. A sub-agent should read **Goal and background**,
**Design decisions** and **Shared context**, then only its own step.

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
  only core monomers.

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
- **Keep the per-page "DB tool" marker, renamed.**
  `AminoAcidTool::UNCLASSIFIED` and `NucleicAcidTool::UNCLASSIFIED` become
  `MONOMER_DATABASE`. They keep their current meaning: this page's last tool
  was its DB monomer. They drive restoring:
  - switching pages restores the new page's DB monomer when that page's tool
    is `MONOMER_DATABASE`, otherwise its standard tool (today's behaviour);
  - toggling atomistic→monomeric restores the DB tool the same way.

  The value isn't mapped to any button in the bimaps, so code must not call
  `.at()` with it.
- **Choosing a monomer switches the side-bar page to match it.**
  - A PEPTIDE monomer switches to AMINO_ACID, and an RNA monomer to
    NUCLEIC_ACID.
  - A CHEM monomer leaves the page as it is, and is stored as the current
    page's DB monomer.
  - Without a canvas selection, choosing sets these together:
    - `DRAW_TOOL = MONOMER_DB_MONOMER`;
    - `MONOMER_TOOL_TYPE`;
    - that page's `*_DB_MONOMER`;
    - that page's `AMINO_ACID_TOOL` / `NUCLEIC_ACID_TOOL = MONOMER_DATABASE`.
  - With a canvas selection, choosing does three things:
    - it mutates the selection by setting that page's `*_DB_MONOMER`;
    - it switches the page;
    - it leaves the draw tool unchanged.

    `setValues()` pings every key it is given, and a ping of
    `AMINO_ACID_TOOL` mutates the selection (`sketcher_widget.cpp`,
    `onModelValuePinged`). So in this case, don't set the page's tool key.
    Check every other key against `onModelValuePinged` before including it.
- **Table selection mirrors the model, and only for the DB tool.** A row is
  selected only when:
  - `getDrawTool() == DrawTool::MONOMER_DB_MONOMER`, and
  - `getMonomerDbMonomer()` is one of the current rows.

  Otherwise no row is selected. No row is selected by default; this overrides
  the spec's "first item should probably be selected".
- **Opening the popup never changes the active tool.** Only these do:
  - choosing a row;
  - clicking a non-blank last-used button.

  A blank last-used button does nothing and looks like empty space.

### Popup and filter behaviour

- **Clicking outside the popup dismisses it, and the click also reaches
  what's underneath** (e.g. the canvas). This matches the existing long-press
  monomer popups, so don't set `WA_NoMouseReplay`.
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
  2. They choose an RNA row, which switches to the NA page.
  3. They close the popup, which records NA.
  4. They reopen it on the NA page, and nothing is reset.

  The filter only resets when the user changes page while the popup is
  closed.

  **Clear All** resets the filter to the default for the **current**
  `MONOMER_TOOL_TYPE`. That can differ from the page at opening if a row
  choice switched pages.
- **Anchor.** A page switch while the popup is open must not move the popup's
  anchor. The side bar's `amino_or_nucleic_stack` sizes itself to its
  largest page, so the shared buttons below it shouldn't move. Verify this.
- **Polymer type is a multi-select combo** with "Peptide", "Nucleic acid"
  and "Structure" (CHEM). Its contents don't depend on the page.
- **Applicability rules.** Structure (CHEM) never affects applicability.
  - The **Monomer type** combo applies only when Peptide is checked and
    Nucleic acid isn't: {Peptide} or {Peptide, Structure}. It only applies
    to amino acids.
  - The **Natural analog** combo applies only when exactly one of Peptide and
    Nucleic acid is checked, with or without Structure.
  - An inapplicable combo is disabled and reset to Any / Any or None, so a
    disabled combo never hides a stale value.
  - Both page defaults therefore start with the Natural analog combo
    enabled. The AA default also starts with Monomer type enabled.
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
  - **Nucleic acid:** `R, P, A, C, G, T, U`. `dR` is omitted, because a sugar
    whose analog is deoxyribose uses `R` in the natural-analog field.
- **Type column:**
  - The table has Symbol, Name, Type and Analog columns. Type shows
    "Peptide", "Nucleic acid" or "Structure".
  - The column is shown when the *filter* allows more than one polymer
    type, and hidden when it allows exactly one. The rows actually displayed
    don't affect it.
  - None analogs (`X`/`N`/empty) display blank.
- **Description text** (Step 5) describes the filter. Its form is
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
- **No "No monomer database found" state.** The table's source list always
  has at least one monomer, because the core DB provides `ac` and `am`, so
  the popup always shows its normal content. Don't add a `QStackedWidget`
  or an empty page.
- **"No monomers match search & filter criteria"** (gray italics) is shown
  when the search and filters leave zero rows. The spec shows it inside the
  empty table with the header still visible, so paint it in the table's
  viewport when there are zero rows (for example, a small `QTableView`
  subclass).
- **The structure image (Step 5) renders the monomer's DB SMILES as-is.**
  Capping groups and atom-map numbers are shown as stored. Nothing is capped
  or converted.

### Shared helpers (extract; don't duplicate)

These come from the user's style rule.

- `show_popup_next_to_widget(QWidget* popup, QWidget* anchor)`. This is the
  positioning logic in `ToolButtonWithPopup::showPopup()`, which is
  protected and button-specific.
- `paint_text_with_fade_out(...)` and `elide_text()`. These are the fade
  painting from `AbstractMonomerItem::paint()`
  (`molviewer/abstract_monomer_item.cpp`).
- `load_monomer_database_from_file(QWidget* parent)`. This is the body of
  `SketcherTopBar::onLoadMonomerDatabaseClicked()`.
- The SMILES → RDKit mol → image path in
  `get_tool_tip_containing_smiles_image()` (Step 5).

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
| Clear All: "Any" everywhere | Resets to the page default above |
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
  - Steps 2–5 also use `implement-ux-spec` and `schrodinger-cpp-style`.
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

    Step 2 changes this to the two per-page keys and `MONOMER_DATABASE` (see
    "Buttons and tool state").
  - **Scene tool.** `Scene::getNewSceneTool()` builds the
    `DrawMonomerSceneTool` from the `MonomerID` alone.
  - **Selection mutation.** With an active canvas selection, a `setValue` or
    ping of the DB-monomer key makes `SketcherWidget::onModelValuePinged`
    mutate the selected monomers (`sketcher_widget.cpp`, `case
    ModelKey::MONOMER_DB_MONOMER`).
  - **`setValues()`** stores *and* pings every key, so any key that
    `onModelValuePinged` handles, such as `AMINO_ACID_TOOL`, also mutates a
    selection.
  - **Don't treat `custom_monomer_btn` / `DrawTool::CUSTOM_MONOMER` as a
    pattern.** Much of that code was meant to be temporary.
- **C++ tests**
  - One executable per `test/**/*.cpp`; they're auto-discovered.
  - Tests that load a DB must reset it on exit. Copy the `ResetDatabase` RAII
    struct from `test/schrodinger/sketcher/widget/test_monomer_tool_widget.cpp`.
- **Playwright (WASM) tests.** Every step that changes the GUI (Steps 2–5)
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
    - Step 2 adds table support: row count, visible headers, the selected
      row, cell text for a row, and a selector that yields a row's rect
      after scrolling it into view.
    - Step 3 adds checkable-combo support: the item check states, plus a
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
    | Right-hand pane (Step 5) | `monomer_db_description_lbl`, `monomer_db_image_lbl` |

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
  that Step 4 can add a core-only analog query without copying it.
- **Tests** go in `test/schrodinger/rdkit_extensions/test_monomer_database.cpp`:
  - `getCustomMonomers()` is empty when only the core DB exists;
  - it returns all custom monomers across polymer types;
  - `getCoreMonomers()` returns the 70 core monomers whether or not a custom
    DB is loaded;
  - `getCustomMonomers()` is empty again after `resetMonomerDefinitions()`.

### 1b. Filter, sort and pure functions (Qt-free)

**New files:** `src/schrodinger/sketcher/model/monomer_database_filter.{h,cpp}`.
- They must include no Qt headers.
- They must not depend on `sketcher_model.h`, which is Qt.

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

    // getters/setters; setPolymerTypes() resets any filter that becomes
    // inapplicable; setNaturalAnalog(NaturalAnalogFilter, std::string
    // specific = {})
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
  - `NONE` matches nothing. The UI never produces it (Step 3), but the API
    stays total.
- **Applicability** is defined once, by the methods above. Everything else
  calls them: `matches()`, the count, the description, and Step 3's combo
  enabling.
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
    reusing existing tables. That header is Qt-free apart from its includes;
    if it isn't, move the needed constants to a Qt-free header rather than
    copying them.
- **Description:** follow the table under "Design decisions". Reuse an
  existing one-letter→three-letter amino-acid map for "Cys". Search the code
  before adding one.
- **Sort**
  - Case-insensitive and stable.
  - Tie-breaks: symbol, then polymer type.
  - Missing values sort last in ascending order.
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
  Peptide or Nucleic acid, and the reset of inapplicable filters;
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

## Step 2: Side-bar buttons and the Monomer Database popup (no filter popup)

**Goal:**
- The shared button pair replaces the Unclassified buttons.
- The DB button opens the popup, which has:
  - a search box;
  - the table;
  - a match count;
  - Close, and Load Database… / Update Database….
- Choosing a row activates that monomer.
- The filter is the page default, with no way to change it yet.
- There's no filter button, description or image yet.

### 2a. Remove the Unclassified buttons; rename the markers; split the model key

- **Delete the buttons.** Remove `aa_unclassified_btn` and
  `na_unclassified_btn` from `ui/monomer_tool_widget.ui`.
- **Rename `AminoAcidTool::UNCLASSIFIED` and `NucleicAcidTool::UNCLASSIFIED`
  to `MONOMER_DATABASE`.** Keep their meaning (see "Buttons and tool
  state"). Trace every use:
  - `sketcher_model.h`;
  - `scene.cpp` (around line 766);
  - `sketcher_widget.cpp` (`nucleic_acid_tool_to_monomer_type`, and the
    `NUCLEIC_ACID_SYMBOL` mutation guard);
  - `sketcher_side_bar.cpp` (`updateCheckState`);
  - `monomer_tool_widget.cpp`;
  - `monomer_symbol_popup_utils.cpp`;
  - the C++ tests.

  If a use existed only to serve the Unclassified popup (for example a
  `NUCLEIC_ACID_SYMBOL` ping carrying `UNCLASSIFIED`) and is now dead, remove
  it instead of renaming it.
- **Split the key.** Replace `ModelKey::MONOMER_DB_MONOMER` with
  `AMINO_ACID_DB_MONOMER` and `NUCLEIC_ACID_DB_MONOMER`.
  - Both default to an empty `MonomerID`.
  - Add `SketcherModel::getMonomerDbMonomer()`.
  - Update `sketcher_model.cpp` (the default-value table and the key list
    around line 94), `Scene::getNewSceneTool()`, the
    `SketcherWidget::onModelValuePinged` mutation case (handle both keys),
    `monomer_symbol_popup_utils.cpp` and the tests.
- **Delete the Unclassified-only code in `MonomerToolWidget`:**
  - the X/N + CHEM popup construction;
  - the `-1` enum-item handling;
  - `getMonomerDbMonomerForButton`.

  Rework the `UNCLASSIFIED` branches of `updateCheckedButton`,
  `onAminoOrNucleicBtnClicked` and `onNucleicAcidClicked` for the shared
  pair (2d). Leave the normal analog-popup construction alone; Step 4
  replaces it.
- **No popups for `unk_btn` / `na_n_btn`.** `ac` and `am` stay reachable
  through the table.
- **Rewrite the affected C++ tests.**
  - Remove or rewrite `unclassified_monomers` and
    `unclassified_chem_monomers` in `test_monomer_tool_widget.cpp`.
  - Rewrite the related cases in `test_sketcher_widget.cpp`,
    `test_sketcher_model.cpp` and the side-bar tests.
  - Their scenarios carry over to the new pair:
    - CHEM reachable from either page;
    - mutation of a selection;
    - each page remembering its own DB monomer;
    - the atomistic↔monomeric toggle restoring the DB tool.

### 2b. Shared helpers

Extract the helpers listed under "Design decisions → Shared helpers", except
the Step 5 image helper. Then make the original call sites use them:
- `ToolButtonWithPopup::showPopup()`;
- `AbstractMonomerItem::paint()`;
- `SketcherTopBar::onLoadMonomerDatabaseClicked()`.

Keep the existing monomer-item rendering and top-bar tests green.

### 2c. New widgets

- **Icons:**
  - Add `icons/monomer_db.svg` to `src/schrodinger/sketcher/sketcher.qrc`.
  - Copy `filter-menu_lb.png`, `filter-menu_lb_act.png`,
    `filter-menu_lb_filtering.png` and `filter-menu_lb_hvr.png` from
    `F:\repos\src\mmshare\python\modules\schrodinger\ui\qt\standard\icons\icon_data\filter-menu`
    into `src/schrodinger/sketcher/icons/`.
  - Add them to the qrc now, ready for Step 3.
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
  - **Header:** follow the mockup, with the title "Monomer Database" and an
    × (`monomer_db_close_btn`).
  - **Content:**
    - **Search box:** a `QLineEdit` with a leading search icon,
      `setClearButtonEnabled(true)` and live filtering on `textChanged`.
      Typing in it must not trigger Sketcher keyboard shortcuts.
    - **Table:** a `QTableView` with:
      - single selection, selecting rows;
      - no editing;
      - no vertical header;
      - fixed row heights;
      - sorting enabled through the model's `sortRequested`;
      - Name column stretched;
      - Type column hidden when the filter allows one polymer type.

      It paints "No monomers match search & filter criteria" (gray italics)
      when it has zero rows.
    - **Match count:** "N matches" ("1 match").
    - **Room** on the right for Step 3's filter button and Step 5's
      description and image.
  - **Bottom row:**
    - `monomer_db_load_link` on the left, with the text "Load Database…"
      when no custom monomers are loaded or "Update Database…" otherwise;
    - "Close" (`monomer_db_close_link`, `TEXT_LINK_STYLE`) on the right.
  - **Load-link visibility:** it follows `setLoadMonomerDatabaseVisible`.
    The path is `SketcherWidget` → `SketcherSideBar` → `MonomerToolWidget` →
    popup. Store the flag so that it applies even before the popup exists.
  - **Data:**
    - Cache `get_monomer_database_table_monomers(getCustomMonomers(),
      getCoreMonomers())` in a `shared_ptr<const vector>`.
    - Refresh the cache on `subscribeToChanges`.
    - The load link says "Load Database…" when `getCustomMonomers()` is
      empty and "Update Database…" otherwise.
    - After a refresh, re-filter, re-sync the selection and update the
      load-link text.
    - Filter and sort changes re-run `filter_and_sort_monomers`.
      `textChanged` re-runs only `filter_by_search_text` on the cached
      filtered rows.
    - After every model reset, re-sync the selection.
  - **API:**
    - `showFromButton(QWidget* anchor, MonomerToolType page)` positions the
      popup with `show_popup_next_to_widget`. Before showing, it applies the
      reset rule from "Design decisions".
    - On every close (×, Close, outside click, Escape, DB button), the popup
      records the current `MONOMER_TOOL_TYPE`. Hook `hideEvent` so that no
      path is missed.
    - Signal `monomerChosen(MonomerID)` fires on user-driven selection
      changes (mouse or keyboard).
    - `syncSelectionToModel(const SketcherModel*)` applies the selection
      rule from "Design decisions":
      - it uses `QSignalBlocker` on the selection model;
      - it calls `scrollTo` for the selected row.

      With a canvas selection, choosing a row mutates the selection, and the
      re-sync then clears the table selection. Clicking the same row again
      therefore re-applies it.
  - **Clicks outside the popup** dismiss it and pass through (don't set
    `WA_NoMouseReplay`).
  - **Bounds:** the popup must stay inside a small canvas on WASM
    (SKETCH-2920).

### 2d. Layout and wiring in `MonomerToolWidget`

- **Layout.** In `ui/monomer_tool_widget.ui`, add a horizontal row between
  `line` and `custom_monomer_btn`: `[monomer_db_last_used_btn]
  [monomer_db_btn]`. Neither button joins a `QButtonGroup`.
- **One activation function.** Write a single
  `activateMonomerDbMonomer(const MonomerID&)` for both the row choice and
  the last-used click. It implements "Choosing a monomer switches the
  side-bar page" from "Design decisions":
  - It picks the target page: PEPTIDE→AMINO_ACID, RNA→NUCLEIC_ACID,
    CHEM→the current page.
  - With a canvas selection, it sets that page's `*_DB_MONOMER` (which
    mutates the selection) and `MONOMER_TOOL_TYPE`. It doesn't set the
    page's tool key, because that ping would also mutate.
  - Otherwise, it sets `DRAW_TOOL`, `MONOMER_TOOL_TYPE`, the page's
    `*_DB_MONOMER` and the page's tool key (`MONOMER_DATABASE`) in one
    `setValues` call.
- **DB-button click:** show the popup for the current `MONOMER_TOOL_TYPE`.
  It doesn't activate anything.
- **Last-used click:**
  - When blank, it does nothing.
  - Otherwise it activates `getMonomerDbMonomer()`. That keeps the current
    page, since the stored monomer belongs to it.
- **`updateCheckedButton()`:**
  - Check `monomer_db_last_used_btn` exactly when
    `draw_tool == MONOMER_DB_MONOMER` and there's no selection. In that
    case, uncheck the page groups.
  - Refresh the last-used text from `getMonomerDbMonomer()`.
  - Call `popup->syncSelectionToModel()`.
- **`onAminoOrNucleicBtnClicked`:** keep today's restore behaviour.
  - If the new page's tool is `MONOMER_DATABASE` and its `*_DB_MONOMER` isn't
    empty, reactivate that DB monomer.
  - Otherwise, activate the page's standard tool.
- **`SketcherSideBar::updateCheckState`:** keep today's atomistic→monomeric
  restore, using the renamed value.
- **DB reload:** if a page's stored `*_DB_MONOMER` is no longer in the table's
  source list:
  - reset it to an empty `MonomerID`, which blanks the button for that page;
  - if that page's DB tool was active, fall back to the page's standard
    tool.

  Check what the current Unclassified code does on reload, and keep that
  behaviour where it still applies.
- **Enabled state:** both buttons are enabled whenever the monomeric tools
  are. The only exception is the blank last-used button.
  - Keep whatever `mutateMonomers` does today for a DB monomer that's
    incompatible with the selection.
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
- **Table contents:**
  - The table holds custom monomers plus core monomers with no natural
    analog, with custom winning on a symbol clash.
  - The AA default shows peptide and CHEM rows. The NA default shows nucleic
    acid and CHEM rows. The Type column is visible.
- **Reset rule (keyed on the page at close):**
  - Reopening on the same page keeps the search text.
  - Opening on a page that differs from the page at the last close clears
    the search text and resets the filter.
  - Choosing an RNA row from a popup opened on the AA page, then closing and
    reopening, does *not* reset.
- **Search:**
  - Search narrows the rows, and the match count is correct.
  - Zero matches shows the placeholder.
- **Choosing a row:**
  - It sets the tool and checks and labels the last-used button.
  - The popup stays open.
  - A peptide switches to the AA page, an RNA monomer to the NA page, and
    CHEM stays on the current page.
  - The chosen monomer is stored in that page's key only.
- **Opening the popup** doesn't change the tool.
- **Last-used button:**
  - Clicking it reactivates the monomer after another tool was chosen.
  - A blank button does nothing.
- **Selection mirroring:**
  - A model change made elsewhere moves the table selection.
  - The selection is cleared for a non-DB tool and for a monomer that isn't
    in the table.
- **With a canvas selection,** choosing a row:
  - mutates the selection;
  - keeps the scene tool;
  - switches the page;
  - updates that page's last-used slot;
  - doesn't ping the page's tool key.
- **DB reload:**
  - It refreshes the table.
  - A monomer that has disappeared blanks that page's button and drops the
    DB tool.
- **Page switching:**
  - Each page restores its own DB monomer, and the last-used button shows
    the current page's value.
  - A page whose last tool was a standard button restores that button.
- **`ac` / `am`** can be chosen from the table and drawn.
- **Long symbol:** a long symbol takes the fade path.
- **`SketcherSideBar`:** an atomistic↔monomeric toggle case.

### Playwright

- **Bridge:** add the table support described under Shared context.
- **Shared helper:** add `chooseFromMonomerDatabase(page, symbol, {search})`
  to `monomer_helpers.js`. Every later test that needs a custom monomer
  should use it.
- **New suite `e2e/monomer_database_popup.test.js`:**
  - opening and closing via ×, Close, the DB button and an outside click;
  - an outside click on the canvas *also* places the active monomer;
  - `ac` / `am` with no DB loaded, and loading a DB through the popup's
    "Load Database…" link;
  - search narrowing the table and the match count;
  - choosing a row and then drawing it, with the HELM checked;
  - choosing an RNA row from the AA page switches the side bar to the NA
    page while the popup stays open and anchored;
  - the last-used label and its fade for a long symbol;
  - reactivation from the last-used button;
  - each page restoring its own DB monomer;
  - the reset rule, keyed on the page at close;
  - typing letters in the search box doesn't switch tools (see
    `popup_shortcuts.test.js`), and shortcuts work after the popup closes;
  - the popup stays within a small viewport;
  - mutating a selection from the table;
  - CHEM chosen from both pages;
  - scrolling to an off-screen row in the ChEMBL DB and drawing it.
- **Update `monomer_database_regressions.test.js`.** It is built around
  `aa_unclassified_btn` and `na_unclassified_btn`. Grep all of `test/wasm`
  for `unclassified`. Rewrite each case against the new popup:
  - the SKETCH-2870 palette-contents, scrolling, per-tab-memory and mutation
    tests. Per-tab memory is kept, now through the shared last-used button.
  - the `expectPalette(page, 'aa_unclassified_btn', …)` assertions in the
    SKETCH-2788 malformed-JSON test.
  - replace the `unclassified-*.png` references.
- **Regenerate** side-bar screenshot references that change because of the
  new row.

---

## Step 3: Filter popup and multi-select combo box

**Goal:**
- the filter button and filter popup from the mockup, without Matching
  SMILES or Apply;
- live filtering;
- a reusable multi-select combo box.

### 3a. `MultiSelectComboBox`

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

### 3b. Filter popup

**Files:** `ui/monomer_database_filter_popup.ui` and
`widget/monomer_database_filter_popup.{h,cpp}`. It's a nested `Qt::Popup`,
positioned below the filter button with `show_popup_next_to_widget`.

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
- "Clear All" (`filter_clear_all_link`) resets the filter to the default
  for the current `MONOMER_TOOL_TYPE`.
- "N filter(s) applied" (`filters_applied_lbl`, gray italics).
- An × close button (`filter_close_btn`) at top right.

**Behaviour:**
- **Live updates:** every change emits `filterChanged(MonomerDatabaseFilter)`
  immediately. The popup never touches the table.
- **Bold:** a combo is bold when its `is*Filtered()` is true, i.e. when it
  differs from the most permissive value.

### 3c. DB popup integration

- **Filter button:** `monomer_db_filter_btn` sits next to the search box. It
  has a dropdown arrow and uses the `filter-menu_lb*` icons:
  - normal, hover and pressed for the ordinary states;
  - `_filtering` (orange) whenever `filter.isFiltering()`.

  It isn't user-toggleable; its look is derived from the filter. Clicking it
  opens the filter popup, initialised from the current filter.
- **`filterChanged`:** store the filter, re-filter, update the Type-column
  visibility and re-sync the selection.
- **Step 4 API:** add `setFilter(const MonomerDatabaseFilter&)`,
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
  - a combo that becomes disabled resets;
  - bold and count values;
  - Clear All returns to the page default;
  - every change re-filters the table live;
  - the button icon state, including orange in the default state and normal
    when all three types are checked;
  - the Type column hides for a single polymer type.
- **Playwright:**
  - **Bridge:** add checkable-combo support.
  - **Extend `monomer_database_popup.test.js`:**
    - opening the filter popup;
    - checking and unchecking polymer types in the dropdown, with the
      dropdown, filter popup and DB popup all staying open (this triple
      popup nesting is the main WASM risk);
    - the last item can't be unchecked;
    - monomer type and analog enabling;
    - live table changes and the count label;
    - Clear All;
    - the orange and normal button states (use the screenshot of the
      button);
    - the Type column visibility.
  - **Update any Step 2 tests** that assumed the filter can't change.

---

## Step 4: Core-only long-press popups and the folder entry

**Goal:**
- the long-press monomer popups show only core monomers;
- each popup also has a folder entry that opens the DB popup, pre-filtered.

- **Core-only data.**
  - Add a core-only query using the parameterised row-reading loop from
    Step 1a, for example `getCoreMonomersByNaturalAnalog(ChainType)`.
  - Use it in `MonomerToolWidget::updateMonomerButtons()`.
  - Don't change `get_merged_na_analogs()`'s `ChainType::DNA` query. If
    core-only data makes touching it unavoidable, stop and tell the user.
- **Build the popups once, in the constructor.**
  - Remove the `subscribeToChanges` subscription, `clear_popups`, and
    `ToolButtonWithPopup::clearPopupWidget()` if nothing else uses it.
  - Turn the `refresh_monomer_popups` test into "popups don't change on DB
    load".
  - Every AA button and every NA base, sugar and phosphate button now gets a
    popup, even with zero analogs, because the folder entry always exists.
  - The exceptions are `unk_btn` (X) and `na_n_btn` (N), which stay
    popup-free. Their monomers are in the table.
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
- **`MonomerToolWidget` on `monomerDatabaseRequested`:**
  - build a filter with exactly that one polymer type, monomer type Any,
    and the given analog;
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
  - popup contents equal the core analogs only, and are unchanged after
    loading a custom DB;
  - the folder button exists, shows the right overlay and isn't in the
    group;
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
    - a normal reopening afterwards keeps that filter.
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

## Step 5: Description and structure image

**Goal:** the right-hand pane of the DB popup.

- **Description label** (`monomer_db_description_lbl`, gray italics), to the
  right of the filter button. It shows `filter.getDescription()` and updates
  live with the filter.
- **Image** (`monomer_db_image_lbl`, in a rounded frame):
  - **What to render:** the selected row's `MonomerInfo::smiles`, **as
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
- **Empty or failed state:** when nothing is selected or the SMILES fails to
  parse, show an empty frame. Never throw from a selection change.
- **Performance:**
  - Cache the image for the current `MonomerID`.
  - Keyboard-scrolling through 12k rows must stay responsive. If needed,
    defer rendering with a 0 ms single-shot timer that coalesces rapid
    changes.
- **C++ tests:**
  - the description for each filter state;
  - the image updating on selection and clearing on deselection;
  - an unparseable SMILES not crashing.
- **Playwright:**
  - the description text through several filter changes;
  - the image appearing for a selected row and clearing when the selection
    clears (screenshot reference of the image region);
  - fast keyboard scrolling through the ChEMBL DB staying responsive.
  - Update earlier suites' popup screenshots, which now include the
    right-hand pane.
