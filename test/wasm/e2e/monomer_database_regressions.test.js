import { expect, test } from '@playwright/test';
import path from 'node:path';
import { setFilePickerResult } from './sketcher/wrappers/sketcher_wasm.js';
import { Sketcher } from './sketcher/wrappers/sketcher.js';
import {
  FIXTURES,
  choosePopup,
  clear,
  clickWidget,
  closeDialog,
  dismissPopup,
  expectCursor,
  expectDialog,
  getExportedHelm,
  getRect,
  pasteText,
  place,
  popupButtons,
  popupInfo,
  requireRect,
  scene,
  screenshotView,
  selectAll,
  waitAtoms,
  widgetState,
} from './e2e_helpers.js';
import { expectAttachments, loadDatabase, openMonomerSketcher } from './monomer_helpers.js';

test.beforeEach(async ({ page }) => {
  await openMonomerSketcher(page);
});
test.setTimeout(120_000);

async function expectPalette(page, button, symbols) {
  expect((await popupButtons(page, button)).map((item) => item.text)).toEqual(symbols);
  await dismissPopup(page);
}

test('SKETCH-2870: defaults and successive database replacements refresh both Unclassified palettes', async ({
  page,
}) => {
  await expect
    .poll(async () => (await widgetState(page, 'aa_unclassified_btn')).visible)
    .toBe(true);
  await clickWidget(page, 'aa_unclassified_btn'); // first click has no previous selection
  await expect
    .poll(async () => (await scene(page)).popup.buttons?.map((item) => item.text).sort())
    .toEqual(['ac', 'am']);
  await dismissPopup(page);
  await expectPalette(page, 'aa_unclassified_btn', ['ac', 'am']);
  await choosePopup(page, 'aa_unclassified_btn', 'ac');
  await expectCursor(page, 'unclassified-ac-cursor.png');
  await place(page);
  expect((await waitAtoms(page, 1))[0].label).toBe('ac');
  await clickWidget(page, 'ala_btn');
  await expectAttachments(page, 0, ['C']);
  await screenshotView(page, 'unclassified-ac.png');
  await clear(page);

  await clickWidget(page, 'nucleic_monomer_btn');
  expect((await widgetState(page, 'na_unclassified_btn')).visible).toBe(false);
  await loadDatabase(page, 'chembl_monomers_no_id.json');
  expect((await widgetState(page, 'na_unclassified_btn')).visible).toBe(false);
  await clickWidget(page, 'amino_monomer_btn');
  const large = await popupButtons(page, 'aa_unclassified_btn');
  expect(large.length).toBeGreaterThan(100);
  expect(large.slice(0, 2).map((item) => item.text)).toEqual(['ac', 'am']);
  await dismissPopup(page);

  await loadDatabase(page, 'chem_monomers.json');
  await expectPalette(page, 'aa_unclassified_btn', ['ac', 'am', 'CHM1', 'CHM2', 'CHM3']);
  await choosePopup(page, 'aa_unclassified_btn', 'CHM2');
  await expectCursor(page, 'unclassified-chem-cursor.png');
  await place(page, -100);
  expect((await waitAtoms(page, 1))[0].label).toBe('CHM2');
  expect(await getExportedHelm(page)).toContain('CHEM1{[CHM2]}');
  await screenshotView(page, 'unclassified-chem.png');
  await clear(page);
  await clickWidget(page, 'nucleic_monomer_btn');
  await expectPalette(page, 'na_unclassified_btn', ['CHM1', 'CHM2', 'CHM3']);
  await choosePopup(page, 'na_unclassified_btn', 'CHM1');
  await expectCursor(page, 'nucleic-unclassified-chem-cursor.png');
  await place(page);
  expect(await getExportedHelm(page)).toContain('CHEM1{[CHM1]}');
  await clear(page);

  await loadDatabase(page, 'na_monomers.json');
  await expectPalette(page, 'na_unclassified_btn', ['NA1', 'NA2']);
  await choosePopup(page, 'na_unclassified_btn', 'NA1');
  await expectCursor(page, 'unclassified-base-cursor.png');
  await place(page);
  expect((await waitAtoms(page, 1))[0].label).toBe('NA1');
  await screenshotView(page, 'unclassified-base.png');
  await clear(page);
  await loadDatabase(page, 'empty_monomers.json');
  expect((await widgetState(page, 'na_unclassified_btn')).visible).toBe(false);
  await clickWidget(page, 'amino_monomer_btn');
  await expectPalette(page, 'aa_unclassified_btn', ['ac', 'am']);
});

test('SKETCH-2870: scroll a large palette to a real off-screen monomer and draw it', async ({
  page,
}) => {
  await loadDatabase(page, 'chembl_monomers_no_id.json');
  const popup = await popupInfo(page, 'aa_unclassified_btn');
  const buttons = popup.buttons;
  const last = buttons.at(-1);
  expect(last.y).toBeGreaterThan(popup.y + popup.height);
  const symbol = last.text;
  await dismissPopup(page);
  await choosePopup(page, 'aa_unclassified_btn', symbol, { scroll: true });
  await place(page);
  expect((await waitAtoms(page, 1))[0].label).toBe(symbol);
  expect(await getExportedHelm(page)).toContain(`[${symbol}]`);
});

test('SKETCH-2870: each tab remembers its own database monomer; tab changes preserve selected content', async ({
  page,
}) => {
  await loadDatabase(page, 'chem_monomers.json');
  await choosePopup(page, 'aa_unclassified_btn', 'CHM2');
  await clickWidget(page, 'nucleic_monomer_btn');
  await choosePopup(page, 'na_unclassified_btn', 'CHM1');
  await clickWidget(page, 'amino_monomer_btn');
  await place(page, -120);
  expect((await waitAtoms(page, 1))[0].label).toBe('CHM2');
  await clickWidget(page, 'nucleic_monomer_btn');
  await place(page, 120);
  expect((await waitAtoms(page, 2)).map((atom) => atom.label)).toEqual(['CHM2', 'CHM1']);
  const before = await getExportedHelm(page);
  await selectAll(page);
  await clickWidget(page, 'amino_monomer_btn');
  await clickWidget(page, 'nucleic_monomer_btn');
  expect(await getExportedHelm(page)).toBe(before);
});

test('SKETCH-2870: choosing an unclassified peptide mutates the selection rather than placing a bead', async ({
  page,
}) => {
  await pasteText(page, 'PEPTIDE1{A}$$$$V2.0');
  await selectAll(page);
  await choosePopup(page, 'aa_unclassified_btn', 'ac');
  expect((await waitAtoms(page, 1))[0].label).toBe('ac');
  expect(await getExportedHelm(page)).toBe('PEPTIDE1{[ac]}$$$$V2.0');
});

test('SKETCH-2788: the upload menu reports success and replaces large analog palettes with native definitions', async ({
  page,
}) => {
  await loadDatabase(page, 'chembl_monomers_no_id.json');
  const large = await popupButtons(page, 'gly_btn');
  expect(large.length).toBeGreaterThan(3);
  expect(large.some((button) => button.text === 'Boc_G')).toBe(true);
  await dismissPopup(page);
  await choosePopup(page, 'gly_btn', 'Boc_G', { scroll: true });
  await place(page);
  expect(await getExportedHelm(page)).toContain('{[Boc_G]}');
  await clear(page);
  await loadDatabase(page, 'native_monomer_definitions.json');
  expect((await widgetState(page, 'gly_btn')).text).toBe('G');
  await expectPalette(page, 'gly_btn', ['G', 'meG']);
  await choosePopup(page, 'gly_btn', 'meG');
  await place(page);
  expect(await getExportedHelm(page)).toContain('{[meG]}');
});

test('SKETCH-2788: malformed JSON explains the failure; picker cancellation leaves content and palettes intact', async ({
  page,
}) => {
  await pasteText(page, 'PEPTIDE1{A}$$$$V2.0');
  const before = await getExportedHelm(page);
  await setFilePickerResult(page, path.join(FIXTURES, 'invalid_monomers.json'));
  await new Sketcher(page).import_menu('load_monomer_database');
  await expectDialog(page, 'Monomer Database Error');
  expect((await widgetState(page, 'text_edit')).text).toMatch(/parse|JSON|invalid/i);
  await closeDialog(page);
  expect(await getExportedHelm(page)).toBe(before);
  await expectPalette(page, 'aa_unclassified_btn', ['ac', 'am']);
  await page.evaluate(() => {
    window.showOpenFilePicker = async () => {
      throw new DOMException('Picker cancelled', 'AbortError');
    };
  });
  await new Sketcher(page).import_menu('load_monomer_database');
  // Reopening a normal menu proves the asynchronous cancellation returned
  // control to the UI. No fixed delay or assertion of silence alone is used.
  await expectPalette(page, 'aa_unclassified_btn', ['ac', 'am']);
  expect(await getExportedHelm(page)).toBe(before);
});

test('SKETCH-2827: a mutation from an Unclassified popup is remembered as the DB monomer of its page without changing the page tool', async ({
  page,
}) => {
  await pasteText(page, 'PEPTIDE1{A}$$$$V2.0');
  await selectAll(page);
  await choosePopup(page, 'aa_unclassified_btn', 'ac');
  expect(await getExportedHelm(page)).toBe('PEPTIDE1{[ac]}$$$$V2.0');
  // Switching pages and back with the selection still active changes nothing
  await clickWidget(page, 'nucleic_monomer_btn');
  await clickWidget(page, 'amino_monomer_btn');
  expect(await getExportedHelm(page)).toBe('PEPTIDE1{[ac]}$$$$V2.0');
  await clickWidget(page, 'clear_selection_btn');

  // The mutation didn't change the page's remembered standard tool
  await clickWidget(page, 'nucleic_monomer_btn');
  await clickWidget(page, 'amino_monomer_btn');
  await expect.poll(async () => (await widgetState(page, 'ala_btn')).checked).toBe(true);

  // The Unclassified button reactivates the remembered DB monomer directly
  await clickWidget(page, 'aa_unclassified_btn');
  await expect
    .poll(async () => (await widgetState(page, 'aa_unclassified_btn')).checked)
    .toBe(true);
  expect((await scene(page)).popup?.buttons?.length ?? 0).toBe(0);
  await place(page, 0, 120);
  expect((await waitAtoms(page, 2)).map((atom) => atom.label)).toEqual(['ac', 'ac']);
});

test('SKETCH-2827: loading a database without the active DB monomer falls back to the standard tool of the page', async ({
  page,
}) => {
  await loadDatabase(page, 'chem_monomers.json');
  await choosePopup(page, 'aa_unclassified_btn', 'CHM2');
  expect((await widgetState(page, 'aa_unclassified_btn')).checked).toBe(true);
  // na_monomers.json has no CHEM monomers
  await loadDatabase(page, 'na_monomers.json');
  await expect.poll(async () => (await widgetState(page, 'ala_btn')).checked).toBe(true);
  expect((await widgetState(page, 'aa_unclassified_btn')).checked).toBe(false);
  await place(page);
  expect((await waitAtoms(page, 1))[0].label).toBe('A');
  expect(await getExportedHelm(page)).toBe('PEPTIDE1{A}$$$$V2.0');
  await clear(page);

  await clickWidget(page, 'nucleic_monomer_btn');
  await choosePopup(page, 'na_unclassified_btn', 'NA1');
  expect((await widgetState(page, 'na_unclassified_btn')).checked).toBe(true);
  // chem_monomers.json has no RNA monomers
  await loadDatabase(page, 'chem_monomers.json');
  await expect.poll(async () => (await widgetState(page, 'na_rna_btn')).checked).toBe(true);
  expect((await widgetState(page, 'na_unclassified_btn')).checked).toBe(false);
  await place(page);
  await waitAtoms(page, 3);
  expect(await getExportedHelm(page)).toBe('RNA1{R(A)P}$$$$V2.0');
});

test('SKETCH-2827: a long-press popup opened near the bottom-right of a small canvas stays inside it', async ({
  page,
}) => {
  await loadDatabase(page, 'chembl_monomers_no_id.json');
  const full = await popupInfo(page, 'aa_unclassified_btn');
  await dismissPopup(page);
  const button = await requireRect(page, 'widget:aa_unclassified_btn');
  // Shrink the canvas so that the popup can't extend right from the button's
  // left edge or fit below the button, but still fits on the canvas
  const width = full.width + Math.floor(button.x / 2);
  const height = Math.max(full.height + 10, Math.round(button.y + button.height + full.height / 2));
  expect(button.x + full.width).toBeGreaterThan(width);
  const viewBefore = await requireRect(page, 'widget:view');
  await page.setViewportSize({ width, height });
  // Qt only sees the new canvas size once the browser's ResizeObserver fires
  await expect
    .poll(async () => {
      const view = await getRect(page, 'widget:view');
      return (
        view !== null && (view.width !== viewBefore.width || view.height !== viewBefore.height)
      );
    })
    .toBe(true);

  const popup = await popupInfo(page, 'aa_unclassified_btn');
  expect(popup.width).toBe(full.width);
  expect(popup.height).toBe(full.height);
  expect(popup.x).toBeGreaterThanOrEqual(0);
  expect(popup.y).toBeGreaterThanOrEqual(0);
  expect(popup.x + popup.width).toBeLessThanOrEqual(width);
  expect(popup.y + popup.height).toBeLessThanOrEqual(height);
  await dismissPopup(page);
});
