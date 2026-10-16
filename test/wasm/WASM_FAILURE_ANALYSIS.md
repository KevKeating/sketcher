# WASM resolved-case failure analysis

Investigated on 2026-10-16 using the [reported GitHub job](https://github.com/KevKeating/sketcher/actions/runs/38832083328/job/116753054785)
and its downloaded WASM app at `F:/Documents/sketcher_wasm/sketcher-wasm`.
The artifact corresponds to commit `f9043b71`. The later hint-label correction
extends only the Playwright scene bridge; it does not change Sketcher behavior.

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

## Certification clarification: hint labels

The owner clarified that a label on the hint fragment's connection qualifies as
the existing monomer's attachment label. The five results below were therefore
test-inspection failures, not certification mismatches in Sketcher behavior.

| Test in `custom_monomer_regressions.test.js` | Expected labels including hint | Initial bridge's unbound labels |
| -------------------------------------------- | ------------------------------ | ------------------------------- |
| SKETCH-2741 custom backbone                  | N, C                           | N                               |
| SKETCH-2741 R3/R4 side-chain chemistry       | N, C, X, R4                    | N, X, R4                        |
| SKETCH-2825 custom base                      | N1/9, H-bond                   | N1/9                            |
| SKETCH-2771 CHEM from amino tab              | R1, R2, R3                     | R2, R3                          |
| SKETCH-2771 CHEM from nucleic tab            | R1, R2, R3                     | R2, R3                          |

The initial bridge returned only visible unbound labels. It now also returns
`hintConnections`, reading the actual `QGraphicsSimpleTextItem` text on each
visible hint. Hidden copies of existing endpoints are matched to visible model
monomers at the same scene positions to report connection owners. All matches
are returned; the tests require exactly the intended owner.

The five tests now check the unbound labels plus the hint text: C/N for peptide
connections, R1/R1 for CHEM connections, and the single shared H-bond label for
base pairs. They do not infer the painted text from molecular properties or use
image comparisons to certify it. A rebuilt WASM artifact is required to run the
updated checks. The downloaded `f9043b71` artifact lacks this bridge field.

## Behavior failure retained

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

The archives follow the repository's existing reference format. Four existing
image checkpoints after the corrected hint assertions have not yet been
generated: custom peptide attachments, custom base attachments, and CHEM
attachments from both tabs. Once the text checks pass on the new build, these
may report expected missing-reference failures. No additional image checks were
added for hint-label text. Windows-generated references require confirmation on
the Linux runner.

## Validation

Validation before the hint-label clarification, across the 52 resolved-case tests:

| Suite           | Passing | Failures before clarification |
| --------------- | ------: | ----------------------------: |
| Custom monomers |      11 |                             5 |
| Connections     |      16 |                             1 |
| Database        |       6 |                             0 |
| Layout          |       5 |                             0 |
| Import/export   |       8 |                             0 |
| Total           |      46 |                             6 |

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
sources or build configuration changed during that initial validation. The new
bridge and text assertions require another WASM build; their browser execution
is pending that artifact.

For the bridge/text-assertion change, the native build and focused
`test/schrodinger/sketcher/tool/test_draw_monomer_scene_tool.exe` test passed.
JavaScript syntax and Playwright discovery also passed. The native build verifies
compilation, not the new browser assertions' runtime results.
