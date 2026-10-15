import { expect } from '@playwright/test';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  clickWidget,
  getExportedHelm,
  getExportedSmiles,
  getRect,
  mouseClick,
  mouseDrag,
  requireRect,
  widgetState,
} from './e2e_helpers.js';
import { Sketcher } from './sketcher/wrappers/sketcher.js';
import { clickPopupRow, setFilePickerResult } from './sketcher/wrappers/sketcher_wasm.js';

export { clickWidget, getExportedHelm, getExportedSmiles, mouseClick, mouseDrag, widgetState };
export const FIXTURES = path.join(
  path.dirname(fileURLToPath(import.meta.url)),
  'fixtures/resolved_cases',
);
export const PEPTIDE = '*NCC(=O)* |$_R1;;;;;_R2$|';
export const BRANCHED_PEPTIDE = '*NC(C*)C(=O)* |$_R1;;;;_R3;;;_R2$|';
export const BASE = '*n1cnc2c(N)ncnc21 |$_R1;;;;;;;;;;$|';
export const CHEM = '*CC(*)C* |$_R1;;;_R2;;_R3$|';

export async function openCase(page, { monomeric = true } = {}) {
  const sk = new Sketcher(page);
  await sk.open();
  await expect
    .poll(() => page.evaluate(() => typeof Module._sketcher_inspect_scene))
    .toBe('function');
  if (monomeric) await clickWidget(page, 'monomeric_btn');
  return sk;
}

// This is a passive observation of the foreground scene, including a modal
// editor. All mutation remains ordinary pointer/keyboard input in the helpers.
export async function scene(page) {
  const state = await page.evaluate(async () => JSON.parse(await Module._sketcher_inspect_scene()));
  state.atoms.sort((a, b) => a.index - b.index);
  return state;
}

export async function waitAtoms(page, count) {
  await expect.poll(async () => (await scene(page)).atoms.length).toBe(count);
  return (await scene(page)).atoms;
}

export const center = (rect) => ({
  x: rect.centerX ?? rect.x + rect.width / 2,
  y: rect.centerY ?? rect.y + rect.height / 2,
});

export async function point(page, index) {
  const atom = (await scene(page)).atoms.find((item) => item.index === index);
  expect(atom, `visible atom ${index}`).toBeTruthy();
  return center(atom);
}

export async function canvasPoint(page, dx = 0, dy = 0) {
  const view = await requireRect(page, 'widget:view');
  return { x: view.x + view.width / 2 + dx, y: view.y + view.height / 2 + dy };
}

export async function place(page, dx = 0, dy = 0) {
  const target = await canvasPoint(page, dx, dy);
  await mouseClick(page, target.x, target.y);
}

export async function clickSelector(page, selector) {
  let rect;
  await expect
    .poll(
      async () => {
        rect = await getRect(page, selector);
        return Boolean(rect?.enabled);
      },
      { message: `clickable ${selector}` },
    )
    .toBe(true);
  const target = center(rect);
  await mouseClick(page, target.x, target.y);
}

export async function dialogTitle(page) {
  return (await getRect(page, 'dialog:title'))?.text ?? null;
}

export async function expectDialog(page, title) {
  await expect.poll(() => dialogTitle(page)).toBe(title);
}

export async function closeDialog(page, button = 'OK') {
  await clickSelector(page, `dialog:${button}`);
}

export async function pasteText(page, text) {
  // Reuse the existing real Import-menu and clipboard workflow. Confirm via
  // the foreground dialog's measured OK button, including in a nested editor.
  await new Sketcher(page).import_menu('paste_in_text', text, false);
  await expect.poll(async () => (await widgetState(page, 'structure_text_edit')).text).toBe(text);
  await closeDialog(page);
}

export async function replaceEditor(page, smiles) {
  await clickWidget(page, 'clear_btn');
  await pasteText(page, smiles);
  await clickWidget(page, 'fit_btn');
}

export async function holdPopup(page, button) {
  const target = center(await requireRect(page, `widget:${button}`));
  await page.mouse.move(target.x, target.y);
  await page.mouse.down();
  // ToolButtonWithPopup uses a 250 ms timer; this wait is the actual gesture.
  await page.waitForTimeout(600);
}

export async function customMenu(page) {
  await holdPopup(page, 'custom_monomer_btn');
  await page.mouse.up();
  await expect.poll(async () => Boolean(await getRect(page, 'menu:Draw Structure...'))).toBe(true);
}

export async function openCustom(page, type = 'peptide') {
  if (type !== 'chem') {
    await clickWidget(page, type === 'peptide' ? 'amino_monomer_btn' : 'nucleic_monomer_btn');
  }
  await customMenu(page);
  await clickSelector(
    page,
    `menu:${type === 'chem' ? 'Draw Structure...' : type === 'peptide' ? 'Sketch Residue...' : 'Sketch Base...'}`,
  );
  await expectDialog(
    page,
    `Define Custom ${type === 'peptide' ? 'Peptide Residue' : type === 'base' ? 'Nucleic Acid Base' : 'Structure'}`,
  );
}

export async function defineCustom(page, type, smiles) {
  await openCustom(page, type);
  await replaceEditor(page, smiles);
  await closeDialog(page);
  await expect.poll(() => dialogTitle(page)).toBe(null);
}

export async function hoverAtom(page, index) {
  const target = await point(page, index);
  await page.mouse.move(target.x, target.y, { steps: 4 });
}

export async function attachments(page, index) {
  await hoverAtom(page, index);
  // Hover items are created on a Qt event turn after mouse movement.
  await page.waitForTimeout(100);
  return (await scene(page)).attachments.filter((item) => item.owner === index);
}

export async function expectAttachments(page, index, labels) {
  await expect
    .poll(async () => (await attachments(page, index)).map((item) => item.text).sort())
    .toEqual([...labels].sort());
}

export async function attachment(page, index, name) {
  let result;
  await expect
    .poll(
      async () => {
        result = (await attachments(page, index)).find((item) => item.name === name);
        return Boolean(result);
      },
      { message: `atom ${index} exposes ${name}` },
    )
    .toBe(true);
  return { x: result.hitX, y: result.hitY };
}

export async function connect(page, first, firstName, second, secondName) {
  const start = await attachment(page, first, firstName);
  const end = await attachment(page, second, secondName);
  await mouseDrag(page, start, end);
}

export async function undo(page) {
  await clickWidget(page, 'undo_btn');
}

export async function redo(page) {
  await clickWidget(page, 'redo_btn');
}

export async function clear(page) {
  await clickWidget(page, 'clear_btn');
  await waitAtoms(page, 0);
}

export async function loadDatabase(page, filename) {
  await setFilePickerResult(page, path.join(FIXTURES, filename));
  await clickWidget(page, 'import_btn');
  await clickPopupRow(page, 'latest', 85);
  await expectDialog(page, 'Monomer Database');
  await expect
    .poll(async () => (await widgetState(page, 'text_edit')).text)
    .toBe(`${filename} read successfully.`);
  await closeDialog(page);
  await expect.poll(() => dialogTitle(page)).toBe(null);
}

export async function popupInfo(page, button) {
  await holdPopup(page, button);
  try {
    let popup;
    await expect
      .poll(async () => {
        popup = (await scene(page)).popup;
        return popup.buttons?.length ?? 0;
      })
      .toBeGreaterThan(0);
    popup.buttons.sort((a, b) => a.y - b.y || a.x - b.x);
    return popup;
  } finally {
    await page.mouse.up();
  }
}

export async function popupButtons(page, button) {
  return (await popupInfo(page, button)).buttons;
}

export async function dismissPopup(page) {
  await page.mouse.up();
  const view = await requireRect(page, 'widget:view');
  await mouseClick(page, view.x + view.width / 2, view.y - 10);
}

export async function choosePopup(page, button, symbol, { scroll = false } = {}) {
  await holdPopup(page, button);
  try {
    let target;
    await expect
      .poll(
        async () => {
          const popup = (await scene(page)).popup;
          target = popup.buttons?.find((item) => item.text === symbol);
          if (!target) return false;
          const pos = center(target);
          const inside =
            pos.y > popup.y &&
            pos.y < popup.y + popup.height &&
            pos.x > popup.x &&
            pos.x < popup.x + popup.width;
          if (!inside && scroll) {
            await page.mouse.move(popup.x + popup.width / 2, popup.y + popup.height / 2);
            await page.mouse.wheel(0, pos.y < popup.y ? -900 : 900);
          }
          return inside;
        },
        { timeout: 15000, message: `${button} contains reachable ${symbol}` },
      )
      .toBe(true);
    const pos = center(target);
    await mouseClick(page, pos.x, pos.y);
  } finally {
    await page.mouse.up();
  }
}

export async function screenshotRegion(page, name, rect) {
  await expect(page).toHaveScreenshot(name, {
    clip: { x: rect.x, y: rect.y, width: rect.width, height: rect.height },
    maxDiffPixelRatio: 0.002,
  });
}

export async function screenshotView(page, name) {
  await screenshotRegion(page, name, await requireRect(page, 'widget:view'));
}

export async function expectCursor(page, name) {
  const pos = await canvasPoint(page);
  await page.mouse.move(pos.x, pos.y);
  let base64;
  await expect
    .poll(async () => {
      base64 = await page.evaluate(() => Module._sketcher_get_cursor_png());
      return base64.length;
    })
    .toBeGreaterThan(0);
  expect(Buffer.from(base64, 'base64')).toMatchSnapshot(name, { maxDiffPixelRatio: 0.002 });
}

export async function relativeLayout(page, order) {
  const atoms = (await scene(page)).atoms;
  const ordered = order ? order.map((index) => atoms.find((atom) => atom.index === index)) : atoms;
  const origin = ordered[0];
  return ordered.map((atom) => ({
    x: atom.sceneX - origin.sceneX,
    y: atom.sceneY - origin.sceneY,
  }));
}

export async function expectLayout(page, expected, order) {
  await expect
    .poll(async () => {
      const actual = await relativeLayout(page, order);
      if (actual.length !== expected.length) return Infinity;
      return Math.max(
        ...actual.map((pos, index) =>
          Math.hypot(pos.x - expected[index].x, pos.y - expected[index].y),
        ),
      );
    })
    .toBeLessThan(0.1);
}
