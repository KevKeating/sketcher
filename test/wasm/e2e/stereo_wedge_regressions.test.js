import { expect, test } from '@playwright/test';
import { Sketcher } from './sketcher/wrappers/sketcher.js';
import {
  clear,
  clickItem,
  clickWidget,
  getExportedSmiles,
  getExportedText,
  loadStructure,
  setClipboardText,
  waitAtoms,
} from './e2e_helpers.js';

// SKETCH-2872: wedges and dashes that are already present on an input molecule
// with 2D coordinates must be kept as-is, rather than being cleared and
// recalculated.  Copy and paste within the Sketcher round-trips through an
// RDKit pickle, which has no MDL wedging information, so it relies on this.

// Alanine.  The bond indices below are 0-based and follow the SMILES order:
//   0: N-C*   1: C*-CH3   2: C*-C(=O)   3: C=O   4: C-OH
// where C* is the stereocenter.
const ALANINE = 'N[C@H](C)C(=O)O';
const ALANINE_ATOM_COUNT = 6;
const STEREOCENTER_TO_METHYL_BOND = 1;
const STEREOCENTER_TO_CARBOXYL_BOND = 2;
const CARBOXYL_C_TO_OH_BOND = 4;

const WEDGE_CFG = 1;
const DASH_CFG = 3;

/**
 * Return every wedged or dashed bond in a V3000 molblock as
 * {bond, begin, end, cfg}, with 0-based bond and atom indices.  The begin atom
 * is included since it determines which end of the bond is narrow.
 */
function stereoBonds(molblock) {
  const lines = molblock.split(/\r?\n/);
  const start = lines.indexOf('M  V30 BEGIN BOND');
  const end = lines.indexOf('M  V30 END BOND');
  return lines
    .slice(start + 1, end)
    .map((line) => line.match(/^M {2}V30 (\d+) \d+ (\d+) (\d+)(?:.* CFG=(\d))?/))
    .filter((match) => match?.[4] !== undefined)
    .map(([, bond, begin, end, cfg]) => ({
      bond: Number(bond) - 1,
      begin: Number(begin) - 1,
      end: Number(end) - 1,
      cfg: Number(cfg),
    }));
}

/**
 * Return the stereochemistry and the wedge/dash placement of the current
 * Sketcher contents.
 */
async function wedgeState(page) {
  return {
    smiles: await getExportedSmiles(page),
    stereoBonds: stereoBonds(await getExportedText(page, 'MDL_MOLV3000')),
  };
}

/**
 * Wait for the bond with the given index to be drawn with the given CFG value
 * (or to be drawn as a plain bond if cfg is undefined).
 */
async function expectBondCfg(page, bond, cfg) {
  await expect
    .poll(
      async () => (await wedgeState(page)).stereoBonds.find((item) => item.bond === bond)?.cfg,
      { message: `bond ${bond} CFG` },
    )
    .toBe(cfg);
}

/** Add alanine through Import -> Paste in Text */
async function importAlanine(sk, page) {
  await sk.import_menu('paste_in_text', ALANINE);
  await waitAtoms(page, ALANINE_ATOM_COUNT);
}

/**
 * Replace the stereocenter's wedge, which the Sketcher places on the methyl
 * bond, with a dash on the carboxyl bond, which isn't where RDKit would put it.
 */
async function moveStereocenterWedge(page) {
  await clickWidget(page, 'stereo_bond2_btn');
  await clickItem(page, 'bond', STEREOCENTER_TO_CARBOXYL_BOND);
  await expectBondCfg(page, STEREOCENTER_TO_CARBOXYL_BOND, DASH_CFG);
  await clickWidget(page, 'single_bond_btn');
  await clickItem(page, 'bond', STEREOCENTER_TO_METHYL_BOND);
  await expectBondCfg(page, STEREOCENTER_TO_METHYL_BOND, undefined);
}

/** More Actions -> Copy All, Clear Sketcher, then More Actions -> Paste */
async function copyAllClearAndPaste(sk, page) {
  const sentinel = '__playwright_waiting_for_copy_all__';
  await setClipboardText(page, sentinel);
  await sk.more_actions_menu('copy_all');
  await sk.wait_for_clipboard_change(sentinel, 'Copy All');
  await clear(page);
  await sk.more_actions_menu('paste');
  await waitAtoms(page, ALANINE_ATOM_COUNT);
}

test.describe('SKETCH-2872: preserving wedges from input molecules', () => {
  let sk;

  test.beforeEach(async ({ page }) => {
    sk = new Sketcher(page);
    await sk.open();
    await importAlanine(sk, page);
  });

  test('a user-added wedge survives Copy All and Paste', async ({ page }) => {
    // These steps match the SKETCH-2872 certification instructions
    await clickWidget(page, 'stereo_bond1_btn');
    await clickItem(page, 'bond', CARBOXYL_C_TO_OH_BOND);
    await expectBondCfg(page, CARBOXYL_C_TO_OH_BOND, WEDGE_CFG);
    const copied = await wedgeState(page);

    await copyAllClearAndPaste(sk, page);
    expect(await wedgeState(page)).toEqual(copied);
  });

  test('a moved stereocenter dash survives Copy All and Paste', async ({ page }) => {
    await moveStereocenterWedge(page);
    const copied = await wedgeState(page);
    expect(copied.stereoBonds).toEqual([
      {
        bond: STEREOCENTER_TO_CARBOXYL_BOND,
        begin: 1,
        end: 3,
        cfg: DASH_CFG,
      },
    ]);

    await copyAllClearAndPaste(sk, page);
    expect(await wedgeState(page)).toEqual(copied);
  });

  test('a moved stereocenter dash survives an RDKit pickle import', async ({ page }) => {
    // This is the scenario from the SKETCH-2872 description, where an RDKit
    // molecule with manually placed wedges is passed to the Sketcher
    await moveStereocenterWedge(page);
    const exported = await wedgeState(page);
    const pickle = await getExportedText(page, 'RDMOL_BINARY_BASE64');

    await clear(page);
    await loadStructure(page, pickle);
    await waitAtoms(page, ALANINE_ATOM_COUNT);
    expect(await wedgeState(page)).toEqual(exported);
  });
});
