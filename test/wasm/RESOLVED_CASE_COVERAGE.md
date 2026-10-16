# Resolved-case browser regressions

These 52 tests follow the Jira certification recipes reviewed on 2026-10-15,
with additional regressions derived from the local fixes. They run against a
WASM artifact built from the same checkout, including `playwright_test_bridge.cpp`.

| Suite in `e2e/`                             | Jira cases                                                                   | Coverage                                                                                                                                                                                                                                                       |
| ------------------------------------------- | ---------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `custom_monomer_regressions.test.js`        | SKETCH-2884, SKETCH-2886, SKETCH-2741, SKETCH-2825, SKETCH-2771, SKETCH-2826 | Menu gestures/text/titles/appearance; custom peptide/base/CHEM creation and cursor hints; attachment validation; edit/save/cancel; compatible backbone preservation; undo/redo.                                                                                |
| `monomer_connection_regressions.test.js`    | SKETCH-2865, SKETCH-2857, SKETCH-2834, SKETCH-2786                           | Bound-site warnings for editing and single/batch context mutations; compatible mutations; native/analog/inline attachment sites; X/N/missing-symbol fallbacks; sulfur-based disulfides; all eight sugar/phosphate drag combinations and an occupied phosphate. |
| `monomer_database_regressions.test.js`      | SKETCH-2870, SKETCH-2788                                                     | UI uploads and messages; database replacement; Unclassified palette contents and visibility; real scrolling; per-tab remembered selections; selected-monomer mutation; malformed JSON and picker cancellation.                                                 |
| `monomer_layout_regressions.test.js`        | SKETCH-2849, SKETCH-2874, SKETCH-2822                                        | Modest distortion and cleanup; forward/reverse chain joining with an offset polymer; layout comparison with fresh import; undo/redo and idempotence; connector routing, redraw, and PNG export.                                                                |
| `monomer_import_export_regressions.test.js` | SKETCH-1848, SKETCH-2862, SKETCH-2861                                        | Original 3D SDF and painted R label; stereochemical round trip; missing identities/invalid inline input; X/N acceptance; inline-SMILES and long-symbol export after layout.                                                                                    |

## Harness boundaries

Actions use mouse/keyboard input and the existing Import/clipboard wrapper.
The bridge reads foreground editor geometry, painted atom chirality labels,
visible attachment labels and hit areas, the actual text labels on hint
connections and their existing monomer owners, popup button geometry, and the actual
View cursor pixmap. Browser screenshots omit the cursor, so cursor assertions
compare that displayed pixmap directly.

The drawing-tool checks count the existing monomer's label on the hint
connection, as clarified by certification. They assert the remaining unbound
labels plus both peptide/CHEM endpoint labels, or the single shared H-bond label.

The connector overlap check uses the connector's hit shape against small,
central regions of **other** beads. Its wider selection halo makes it unsuitable
for testing its own endpoints; focused visual references cover endpoint routing.

File tests replace only the result of `showOpenFilePicker`, using the existing
`setFilePickerResult` helper. They exercise the UI menu, Qt callback, parsing,
messages, and palette refresh. They do not certify native picker filters.

Import rejection preserves existing content in **Add mode**. The current
replacement path clears the document before validation, so preservation in
replacement mode is not asserted.

SKETCH-2861's certification expected SMILES omits sulfur present in its input.
The test instead uses the fix's sulfur-containing atomistic reference
`O=C([C@H](CSCCN)N)O`, canonicalized independently of monomer layout.

## Fixtures

`e2e/fixtures/resolved_cases/` contains versioned copies of the certification
inputs so tests do not depend on Jira access or untracked local files:

- `mol2-1.sdf`: original SKETCH-1848 attachment, supplied by Kevin.
- `chembl_monomers_no_id.json`, `chem_monomers.json`, `na_monomers.json`, and
  `empty_monomers.json`: local copies of the SKETCH-2870/SKETCH-2788 inputs.
- `native_monomer_definitions.json`: local copy of the SKETCH-2788 reload input.
- `long_symbol.json`: minimal LongAla definition from the SKETCH-2861 C++ regression.
- `invalid_monomers.json`: intentionally malformed JSON for the upload error path.

## Remote execution and visual references

From `test/wasm`, run just these suites against the freshly built artifact:

```bash
npx playwright test e2e/custom_monomer_regressions.test.js e2e/monomer_connection_regressions.test.js e2e/monomer_database_regressions.test.js e2e/monomer_layout_regressions.test.js e2e/monomer_import_export_regressions.test.js
```

Visual references were generated locally on 2026-10-16 against the downloaded
artifact from run `38832083328` (commit `f9043b71`). The 23 PNGs are stored in five
suite archives under `e2e/reference_archives/`. See
[WASM_FAILURE_ANALYSIS.md](WASM_FAILURE_ANALYSIS.md) for the remaining HELM export
failure and the corrected interpretation of hint labels. Four image checkpoints
after the corrected hint assertions remain ungenerated.

The revised hint-label checks require a new WASM build containing the updated
scene bridge. After downloading that artifact, serve its directory on port 8000,
then run the command above with
`SKETCHER_WASM_APP_DIR` set to that directory. On Windows this also avoids
depending on a `python3` command in PATH: start the server separately with an
available Python interpreter. Playwright reuses an existing server outside CI.

To regenerate references later, reuse a build's WASM artifact with the existing
**Update Wasm Snapshots** workflow, or add `--update-snapshots` to the command
above. Review the resulting PNGs against the Jira recipes; generation alone does
not establish correctness. These references use the same Playwright/Chromium
version as the reported Linux run, but were generated on Windows; Linux visual
parity still needs the next CI run.

In a checkout with GNU tar available, extract the existing references before
copying in the newly generated `e2e/__snapshots__/` directories:

```bash
node --input-type=module -e "import setup from './e2e/sketcher/scripts/prepare_runtime_fixtures.mjs'; setup();"
```

Then copy in the newly generated references and run `npm run archive:sketcher-snapshots`.
Commit the resulting `reference_archives/*.tar.gz` files. A normal subsequent run
must pass without `--update-snapshots`. No further WASM compilation is needed to
generate or review references from the same artifact.

## Exclusions

- SKETCH-2895, SKETCH-2856, SKETCH-2855 require Maestro/LiveDesign host integration.
- SKETCH-2823 changes the browser-native file chooser's default filter. Mocking
  the selected file would bypass the behavior under certification.
