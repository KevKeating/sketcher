# WASM resolved-case failure analysis

Investigated on 2026-10-16 using the [reported GitHub job](https://github.com/KevKeating/sketcher/actions/runs/38832083328/job/116753054785)
and its downloaded WASM app at `F:/Documents/sketcher_wasm/sketcher-wasm`.
The artifact corresponds to commit `f9043b71`. No application code was changed
during this investigation.

## Test defects corrected

| Affected tests                                                                                                               | Cause and correction                                                                                                                                                                                                          |
| ---------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Custom appearance, R3/R4, base/CHEM definition, selection mutation, validation, and all three custom structure editing tests | `replaceEditor` unconditionally clicked Clear on an empty editor, where Clear is correctly disabled. It now clears only a populated editor.                                                                                   |
| Custom backbone drawing and subsequent attachment checks                                                                     | Placement left the pointer at the newly created bead. Re-entering the bead explicitly delivers the hover event and exposes the actual attachment-label discrepancy below.                                                     |
| SKETCH-2865 bound-R2 edit                                                                                                    | Alanine is already active on startup; clicking it again opens the analog palette, which consumes the drawing drag. The test now verifies that the tool is already active.                                                     |
| SKETCH-2865 batch mutation; SKETCH-2870 selected-monomer mutation; SKETCH-2741 custom selection mutation                     | The canvas-only keyboard focus helper followed by Ctrl+A did not select anything. Tests now click the visible All control and verify the resulting selection state.                                                           |
| SKETCH-2870 database replacement/scrolling and SKETCH-2788 database reload                                                   | The new helper clicked an import-menu row immediately after opening the menu, unlike the existing wrapper's settled-menu workflow. It now reuses that wrapper and allows 30 seconds for a large database's completion dialog. |
| SKETCH-2788 picker cancellation (additional local failure)                                                                   | A widget lookup immediately after asynchronous cancellation could transiently return no rectangle. Press-and-hold setup now reuses the existing clickable-state polling helper.                                               |
| SKETCH-2849 cleanup distortion; SKETCH-2822 redraw after dragging                                                            | A selection drag selects an area instead of moving a bead. Moving with no selection translates the whole drawing. Tests now select one bead, move that selection, clear it, and verify that relative coordinates changed.     |
| SKETCH-2874 reverse-chain joining with offset 1                                                                              | The new chain overlapped the lower imported chain. Hovering the imported cysteine hit a newer overlapping bead. The new chain is now drawn in separate empty space.                                                           |

An additional intermittent SKETCH-1848 failure appeared during archive
verification: scene inspection tried to parse a transient empty bridge result
during the asynchronous SDF import. It now waits for a nonempty response, as the
existing geometry helper does. The SDF test also reuses the file-import wrapper.

## Behavior failures retained

### Attachment labels in the custom drawing tool: five tests

| Test in `custom_monomer_regressions.test.js` | Certification/expected visible labels | Actual visible labels |
| -------------------------------------------- | ------------------------------------- | --------------------- |
| SKETCH-2741 custom backbone                  | N, C                                  | N                     |
| SKETCH-2741 R3/R4 side-chain chemistry       | N, C, X, R4                           | N, X, R4              |
| SKETCH-2825 custom base                      | N1/9, H-bond                          | N1/9                  |
| SKETCH-2771 CHEM from amino tab              | R1, R2, R3                            | R2, R3                |
| SKETCH-2771 CHEM from nucleic tab            | R1, R2, R3                            | R2, R3                |

This is a discrepancy with the explicit hover instructions on
[SKETCH-2741](https://schrodinger.atlassian.net/browse/SKETCH-2741),
[SKETCH-2825](https://schrodinger.atlassian.net/browse/SKETCH-2825), and
[SKETCH-2771](https://schrodinger.atlassian.net/browse/SKETCH-2771).
It does not demonstrate that the chemical attachment is absent.
`DrawMonomerSceneTool::updateHoveredUnboundAP` explicitly hides the attachment
label selected for the default connection preview and draws a hint fragment
instead. Thus this could be intentional UI behavior with outdated certification
wording. The tests still require the certified visible labels; no application
change or expectation relaxation was made. Their later image/connection
checkpoints are not reached.

### SKETCH-2865: removing a backbone connection leaves incorrect HELM chain identity

Test: `editing a bound R2: Cancel returns to the edited drawing; OK removes the bond`
in `monomer_connection_regressions.test.js`.

The warning and Cancel flow work. Accepting the warning creates a custom bead
and removes the displayed connector, as certified. The test confirms that there
are zero scene connectors. However, HELM export still serializes both disconnected
beads as one `PEPTIDE1` polymer, using an implicit backbone connection:

```text
PEPTIDE1{[*N[C@@H](C)C=O |$_R1;;;;;$,atomProp:1.pdbName. N  :2.pdbName. CA :3.pdbName. CB :4.pdbName. C  :5.pdbName. O  ,a:2|].A}$$$$V2.0
```

The expected representation has two peptide polymers, since R2 and its backbone
bond were removed. The exported inline monomer does not even define R2.
This is an additional serialization regression beyond the case's visual
certification. The HELM writer groups by stored polymer identity and writes
adjacent monomers with dots; the removal path leaves those identities together.
No fix was made to the model, removal path, or writer.

## Missing image references

Five remote failures were solely missing references: native attachment sites,
custom sulfur disulfide, the original 3D SDF, intrachain routing/redraw/export,
and adjacent backbone-plus-side-chain routing. The custom appearance and database
replacement tests also reported missing images alongside their test defects.

The artifact is sufficient for local execution. Playwright 1.64.1 / Chromium
1248 matches the reported run. Generated and visually reviewed 23 reachable PNGs:

| Archive                                            | Images |
| -------------------------------------------------- | -----: |
| `custom_monomer_regressions.test.js.tar.gz`        |      9 |
| `monomer_connection_regressions.test.js.tar.gz`    |      2 |
| `monomer_database_regressions.test.js.tar.gz`      |      7 |
| `monomer_import_export_regressions.test.js.tar.gz` |      1 |
| `monomer_layout_regressions.test.js.tar.gz`        |      4 |

The archives follow the repository's existing reference format. Images beyond
the retained failing assertions were not fabricated or accepted as baselines.
They still need generation after the relevant behavior or certification is
resolved. Windows-generated references require confirmation on the Linux runner.

## Validation

Final validation across the 52 resolved-case tests:

| Suite           | Passing | Retained behavior failures |
| --------------- | ------: | -------------------------: |
| Custom monomers |      11 |                          5 |
| Connections     |      16 |                          1 |
| Database        |       6 |                          0 |
| Layout          |       5 |                          0 |
| Import/export   |       8 |                          0 |
| Total           |      46 |                          6 |

The full 52-test run initially confirmed this split. A subsequent archive-based
run exposed the intermittent import-readiness error described above. After its
correction, the other four suites again confirmed 38 passes and the same six
behavior failures. The corrected import/export suite then passed twice (16 test
executions). These verification runs used the packaged references without
snapshot updates.

Verified every archived PNG against its generated file, including archive entry
counts and normalized timestamps/ownership. JavaScript syntax, Prettier checks,
and both staged and unstaged `git diff --check` passed.

The entire pre-existing browser suite and native C++ tests were not run; no C++
sources or build configuration changed in this investigation.
