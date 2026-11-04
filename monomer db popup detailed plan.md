# Monomer Database popup: implementation plan (SKETCH-2827)

This is the single source of truth for SKETCH-2827. It replaces
`monomer db popup initial plan.md`. The work is split into five steps. Each step
is one self-contained brief for a sub-agent, and each ends in a building,
tested, reviewable state. A sub-agent should read **Goal and background**,
**Design decisions** and **Shared context**, then only its own step.

Items marked **Pending confirmation** are new decisions raised by the latest
round of feedback. The plan follows the stated default until the user says
otherwise.

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
  - The docx also has an empty-state mockup ("No monomer database found",
    "Load Database…", "Close").
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

- **The table contains custom monomers only.** Core monomers
  (`m_core_monomers_db`) never appear, so the core/custom de-duplication
  problem goes away. Core monomers stay reachable through the side-bar
  buttons and their long-press popups.
- **One read per database change; everything else happens in memory.**
  - `MonomerDatabase` gets a single new public call, `getCustomMonomers()`.
  - Filtering, text search and sorting are pure, Qt-free functions over
    the cached vector.
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

  This follows the `custom_monomer_btn` precedent:
  - the pair isn't in `amino_monomer_group` or `nucleic_monomer_group`;
  - the last-used button is checked exactly when the DB tool is active and
    there's no canvas selection;
  - the page's buttons are unchecked while it is.
- **The "Unclassified" buttons and the `AminoAcidTool::UNCLASSIFIED` /
  `NucleicAcidTool::UNCLASSIFIED` enum values are removed in Step 2.** They
  are removed, not renamed: a shared button outside the page groups has no
  use for a per-page marker.

  **Pending confirmation.** Today each page remembers its own DB monomer,
  which SKETCH-2870 tests. With one shared button, the plan follows the
  custom-monomer precedent:
  - switching pages while the DB tool is active activates the new page's
    standard tool;
  - toggling atomistic→monomeric activates `DrawTool::MONOMER`.

  The alternative is to keep the DB tool active across page switches.
- **The last-used slot is the model's `ModelKey::MONOMER_DB_MONOMER` value.**
  It isn't a separate widget member. `SketcherModel::setValues()` stores the
  value and also pings it, so one `setValue` call does both jobs:
  - with a canvas selection, it mutates the selection and updates the
    last-used slot, and doesn't change the scene tool;
  - without one, it is set together with `DRAW_TOOL = MONOMER_DB_MONOMER`.
- **Choosing a monomer doesn't change `MONOMER_TOOL_TYPE` (the side-bar
  page).** The DB scene tool takes its chain type from the `MonomerID`, so the
  page is irrelevant to drawing. Switching pages would also reset the popup's
  filter on its next opening.
- **Table selection mirrors the model, and only for the DB tool.** A row is
  selected only when:
  - `getDrawTool() == DrawTool::MONOMER_DB_MONOMER`, and
  - the `MONOMER_DB_MONOMER` monomer is one of the current rows.

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
- **Popup state and reset rule.** One popup instance exists. It remembers
  the page (`MonomerToolType`) it was last opened from. On opening:
  - **First opening, or the page differs from the last opening:**
    - clear the search text;
    - reset the filter to the page default:
      - AMINO_ACID: polymer type {Peptide, Structure}, monomer type Any,
        natural analog Any or None;
      - NUCLEIC_ACID: polymer type {Nucleic acid, Structure}, Any, Any or
        None.
  - **Otherwise:** the search text, filter and sort are kept.

  **Clear All** in the filter popup resets the filter to the default for the
  last-opened page.
- **Polymer type is a multi-select combo** with "Peptide", "Nucleic acid"
  and "Structure" (CHEM). Its contents don't depend on the page.
- **Applicability rules:**
  - The **Monomer type** combo applies only when the polymer types are
    exactly {Peptide}.
  - The **Natural analog** combo applies only when they are exactly
    {Peptide} or exactly {Nucleic acid}.
  - An inapplicable combo is disabled and reset to Any / Any or None, so a
    disabled combo never hides a stale value.

  **Pending confirmation:** with the new defaults, both combos are **disabled
  when the popup first opens**, because Structure is also checked. The user
  has to uncheck Structure to filter by monomer type or analog.
- **"Filtering" baseline.** A filter is "filtering" when it differs from
  showing everything: all three polymer types, Any, and Any or None.
  - While it's filtering, the filter button looks pressed (orange
    `filter-menu_lb_filtering.png`). Otherwise it looks normal.
  - This holds even in the page-default state.

  **Pending confirmation:** use the same baseline for the "N filters applied"
  count and for bolding a combo. Count:
  - 1 if the polymer types aren't all three;
  - +1 if an applicable monomer type isn't Any;
  - +1 if an applicable analog isn't Any or None.

  A combo is bold when it contributes to the count. The page-default state
  therefore reads "1 filter applied" with Polymer type in bold, consistent
  with the orange button.

  The previously approved rule ("count combos that differ from Clear All")
  was agreed when Clear All meant "show everything". Under the new Clear All,
  it would show "0 filters applied" next to an orange button.
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
  - **Pending confirmation:** the Type column is hidden when the *filter*
    allows exactly one polymer type. The alternative is to hide it when the
    *visible rows* happen to have one type; that would flicker while typing.
  - None analogs (`X`/`N`/empty) display blank.
- **Description text** (Step 5) has the form
  `<polymer types>[, <monomer type>][, <analog>]`.

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
  | Peptide + analog None | `Peptide, unclassified monomers` |
  | Peptide + analog Any | `Peptide, classified monomers` (new wording) |

- **Load Database… / Update Database… links:**
  - "Load Database…" is shown in the empty state and "Update Database…"
    otherwise, both at bottom-left as in the mockups.
  - They're shown by default.
  - `SketcherWidget::setLoadMonomerDatabaseVisible(false)` hides them, along
    with the import menu's "Load Monomer Database…". The WASM JS API
    `sketcher_load_custom_monomers` (`src/app/main.cpp`) calls that
    automatically after a host page loads monomers.
  - Both links and the import-menu action run the same extracted function.
- **"No monomer database found" state:**
  - It's shown whenever there are no custom monomers.
  - It's a `QStackedWidget` whose content page holds everything above the
    bottom link row: search, filter button, description, table, match
    count and image.
  - The empty page holds the centered gray-italic message.
  - The bottom row (load/update link and Close) sits outside the stack.
- **"No monomers match search & filter criteria"** is a separate state
  (gray italics). The spec shows it inside the empty table with the header
  still visible, so paint it in the table's viewport when there are zero
  rows (for example, a small `QTableView` subclass). It isn't a stack page.
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
| Table includes all monomers; first item selected by default | Custom monomers only; no default selection; the selection mirrors the active DB tool |
| Opening the popup loads the last-used item | It doesn't. Only the last-used button does |
| Peptide analog list includes X and omits N | Includes N and omits X (X is "None") |
| No Type column | A Type column, hidden when the filter allows one polymer type |
| Image shows a capped structure | The DB SMILES as stored, with capping groups and atom-map numbers |
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
  - **A DB monomer is active** when `getDrawTool() ==
    DrawTool::MONOMER_DB_MONOMER` and `ModelKey::MONOMER_DB_MONOMER` holds an
    `rdkit_extensions::MonomerID`.
  - **Scene tool.** `Scene::getNewSceneTool()` builds the
    `DrawMonomerSceneTool` from the `MonomerID` alone.
  - **Selection mutation.** With an active canvas selection, a `setValue` or
    ping of `MONOMER_DB_MONOMER` makes `SketcherWidget::onModelValuePinged`
    mutate the selected monomers (`sketcher_widget.cpp`, `case
    ModelKey::MONOMER_DB_MONOMER`).
  - **`setValues()`** stores *and* pings every key.
  - **Precedent.** `custom_monomer_btn` and `DrawTool::CUSTOM_MONOMER` are
    the existing pattern for a shared, non-grouped monomer button.
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
    | Popup contents | `monomer_db_search_le`, `monomer_db_table`, `monomer_db_match_count_lbl`, `monomer_db_content_stack`, `monomer_db_empty_lbl` |
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

### 1a. `MonomerDatabase::getCustomMonomers()` (rdkit_extensions)

- **Add the method.** Add `[[nodiscard]] std::vector<MonomerInfo>
  getCustomMonomers() const;`.
  - It returns every monomer in the custom DB, of all polymer types.
  - It returns an empty vector when no custom DB is loaded.
- **Refactor, don't copy.** Refactor the private `getMonomers(ChainType,
  bool)` so that both functions share the row-reading loop (prepare, step,
  `assign_monomer_info`). Parameterize it by which DB handle(s) to read, so
  that Step 4 can add a core-only query without copying it.
- **Tests** go in `test/schrodinger/rdkit_extensions/test_monomer_database.cpp`:
  - empty when only the core DB exists;
  - returns all custom monomers across polymer types;
  - a custom monomer that shadows a core symbol appears once, with its
    custom fields;
  - empty again after `resetMonomerDefinitions()`.

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
    bool isMonomerTypeApplicable() const;   // polymer types == PEPTIDE
    bool isNaturalAnalogApplicable() const; // == PEPTIDE or == NUCLEIC_ACID
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
  `Terminal`.
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
- every polymer-type combination, including NONE and ALL;
- applicability for every combination, and the reset of inapplicable
  filters;
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
  - Close, Load Database… / Update Database…, and the empty state.
- Choosing a row activates that monomer.
- The filter is the page default, with no way to change it yet.
- There's no filter button, description or image yet.

### 2a. Remove the Unclassified buttons and the UNCLASSIFIED markers

- **Delete the buttons.** Remove `aa_unclassified_btn` and
  `na_unclassified_btn` from `ui/monomer_tool_widget.ui`.
- **Delete `AminoAcidTool::UNCLASSIFIED` and `NucleicAcidTool::UNCLASSIFIED`.**
  Trace every use:
  - `sketcher_model.h`;
  - `scene.cpp` (around line 766);
  - `sketcher_widget.cpp` (`nucleic_acid_tool_to_monomer_type`, and the
    `NUCLEIC_ACID_SYMBOL` mutation guard);
  - `sketcher_side_bar.cpp` (`updateCheckState`: the atomistic→monomeric
    toggle now picks `DrawTool::MONOMER`);
  - `monomer_tool_widget.cpp`;
  - `monomer_symbol_popup_utils.cpp`;
  - the C++ tests.
- **Delete the Unclassified-only code in `MonomerToolWidget`:**
  - the X/N + CHEM popup construction;
  - the `-1` enum-item handling;
  - `getMonomerDbMonomerForButton`;
  - the Unclassified branches of `updateCheckedButton`,
    `onAminoOrNucleicBtnClicked` and `onNucleicAcidClicked`.

  Leave the normal analog-popup construction alone; Step 4 replaces it.
- **Keep `ac` and `am` reachable. Pending confirmation.** The core terminal
  peptides `ac` and `am` (analog `X`) are reachable today *only* through the
  Unclassified popup. The table excludes core monomers, so removing that
  popup would strand them.
  - Default: give `unk_btn` and `na_n_btn` long-press popups of
    None-analog monomers, built the same way as the other analog popups.
  - Both are currently plain `QToolButton`s, so they must become
    `ModularToolButton`s.
  - In Step 4 they become core-only, plus the folder entry.
- **Rewrite the affected C++ tests.**
  - Remove or rewrite `unclassified_monomers` and
    `unclassified_chem_monomers` in `test_monomer_tool_widget.cpp`.
  - Rewrite the related cases in `test_sketcher_widget.cpp`,
    `test_sketcher_model.cpp` and the side-bar tests.
  - Their scenarios carry over to the new pair where they still apply:
    - CHEM reachable from either page;
    - mutation of a selection.

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
  - **Display:** it shows the symbol of the model's `MONOMER_DB_MONOMER`
    value.
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
  - **`monomer_db_content_stack`** has two pages.
    - **Content page:**
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
    - **Empty page:** "No monomer database found", centered, in gray
      italics.
  - **Bottom row (outside the stack):**
    - `monomer_db_load_link` on the left, with the text "Load Database…" in
      the empty state or "Update Database…" otherwise;
    - "Close" (`monomer_db_close_link`, `TEXT_LINK_STYLE`) on the right.
  - **Load-link visibility:** it follows `setLoadMonomerDatabaseVisible`.
    The path is `SketcherWidget` → `SketcherSideBar` → `MonomerToolWidget` →
    popup. Store the flag so that it applies even before the popup exists.
  - **Data:**
    - Cache `getCustomMonomers()` in a `shared_ptr<const vector>`.
    - Refresh the cache on `subscribeToChanges`.
    - After a refresh, re-filter, re-sync the selection and switch the stack
      page.
    - Filter and sort changes re-run `filter_and_sort_monomers`.
      `textChanged` re-runs only `filter_by_search_text` on the cached
      filtered rows.
    - After every model reset, re-sync the selection.
  - **API:**
    - `showFromButton(QWidget* anchor, MonomerToolType page)` applies the
      reset rule from "Design decisions" and positions the popup with
      `show_popup_next_to_widget`.
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
  the last-used click.
  - With a canvas selection: `setValue(MONOMER_DB_MONOMER, id)`. This
    mutates the selection, updates the last-used slot and leaves the tool
    unchanged.
  - Otherwise: `setValues({DRAW_TOOL = MONOMER_DB_MONOMER,
    MONOMER_DB_MONOMER = id})`. `MONOMER_TOOL_TYPE` is unchanged.
- **DB-button click:** show the popup with the current `MONOMER_TOOL_TYPE`.
  It doesn't activate anything.
- **`updateCheckedButton()`:**
  - Check `monomer_db_last_used_btn` exactly when
    `draw_tool == MONOMER_DB_MONOMER` and there's no selection. In that
    case, uncheck the page groups, as for `CUSTOM_MONOMER`.
  - Refresh the last-used text from the model.
  - Call `popup->syncSelectionToModel()`.
- **`onAminoOrNucleicBtnClicked`:** the DB-restore branch goes away. A page
  switch activates the page's standard tool. This is pending confirmation.
- **DB reload:** if the stored `MONOMER_DB_MONOMER` is no longer in the
  custom DB:
  - reset it to an empty `MonomerID`, which blanks the button;
  - if the DB tool was active, fall back to `DrawTool::MONOMER`.

  Check what the current Unclassified code does on reload, and keep that
  behaviour where it still applies.
- **Enabled state:** mirror `custom_monomer_btn`'s
  `updateWidgetsEnabled()` logic. Keep whatever `mutateMonomers` does today
  for a DB monomer that's incompatible with the selection.

### C++ tests

Add these to `test_monomer_tool_widget.cpp`, or put popup-only behaviour in a
new `test_monomer_database_popup.cpp`.
- **Empty state:**
  - No custom DB shows the empty page with "Load Database…".
  - Loading a DB shows the content page with "Update Database…".
  - `setLoadMonomerDatabaseVisible(false)` hides the link.
- **Table contents:**
  - Only custom monomers appear.
  - The AA default shows peptide and CHEM rows. The NA default shows nucleic
    acid and CHEM rows. The Type column is visible.
- **Reset rule:**
  - Reopening from the same page keeps the search text.
  - Opening from the other page clears the search text and resets the
    filter.
- **Search:**
  - Search narrows the rows, and the match count is correct.
  - Zero matches shows the placeholder.
- **Choosing a row:**
  - It sets the tool and checks and labels the last-used button.
  - The popup stays open.
  - `MONOMER_TOOL_TYPE` is unchanged.
- **Opening the popup** doesn't change the tool.
- **Last-used button:**
  - Clicking it reactivates the monomer after another tool was chosen.
  - A blank button does nothing.
- **Selection mirroring:**
  - A model change made elsewhere moves the table selection.
  - The selection is cleared for a non-DB tool and for a monomer that isn't
    in the table.
- **With a canvas selection,** choosing a row mutates the selection, keeps
  the scene tool and updates the last-used slot.
- **DB reload:**
  - It refreshes the table.
  - A monomer that has disappeared blanks the button and drops the DB tool.
- **Page switching** while the DB tool is active (the new behaviour).
- **`unk_btn` popup:** it contains `ac` and `am`.
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
  - the empty state, and loading a DB through the popup's link;
  - search narrowing the table and the match count;
  - choosing a row and then drawing it, with the HELM checked;
  - the last-used label and its fade for a long symbol;
  - reactivation from the last-used button;
  - the reset rule across page switches;
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
    tests. Per-tab memory becomes the new page-switch behaviour.
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
  last checked item is ignored. **Pending confirmation:** the alternative is
  to allow zero, giving an empty table.
- **Display text:** paint the checked items joined with ", ", in item order.
  **Pending confirmation:** show "Any" when every item is checked.
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
- "Clear All" (`filter_clear_all_link`) resets the filter to the
  last-opened page's default.
- "N filter(s) applied" (`filters_applied_lbl`, gray italics).
- An × close button (`filter_close_btn`) at top right.

**Behaviour:**
- **Live updates:** every change emits `filterChanged(MonomerDatabaseFilter)`
  immediately. The popup never touches the table.
- **Bold:** a combo is bold when its `is*Filtered()` is true (pending
  confirmation; see "Design decisions").

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
  - the monomer-type and analog combos are disabled in the default state
    and enabled for exactly {Peptide};
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
  - This includes `unk_btn` and `na_n_btn`; see Step 2a.
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
    - SPECIFIC `R` for dR;
    - None for X and N.
- **`MonomerToolWidget` on `monomerDatabaseRequested`:**
  - build a filter with exactly that one polymer type, monomer type Any,
    and the given analog;
  - `setFilter` it and `clearSearchText()`;
  - record the current page as the popup's last-opened page, so that the
    next ordinary opening doesn't reset;
  - open the popup from **`monomer_db_btn`**, not from the long-press popup.

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
    - the folder entry from a peptide button, a base, dR (filtered to `R`)
      and X (filtered to None);
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
