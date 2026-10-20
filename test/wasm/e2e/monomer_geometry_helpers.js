import { expect } from '@playwright/test';
import { scene } from './e2e_helpers.js';

export function rectanglesOverlap(a, b, tolerance = 0.01) {
  return (
    Math.min(a.x + a.width, b.x + b.width) - Math.max(a.x, b.x) > tolerance &&
    Math.min(a.y + a.height, b.y + b.height) - Math.max(a.y, b.y) > tolerance
  );
}

// Return the portion of a line inside a rectangle, as fractions of its length.
function clippedInterval([start, end], rect) {
  let low = 0;
  let high = 1;
  for (const [axis, size] of [
    ['x', 'width'],
    ['y', 'height'],
  ]) {
    const delta = end[axis] - start[axis];
    if (Math.abs(delta) < 1e-9) {
      if (start[axis] < rect[axis] || start[axis] > rect[axis] + rect[size]) return null;
    } else {
      const first = (rect[axis] - start[axis]) / delta;
      const second = (rect[axis] + rect[size] - start[axis]) / delta;
      low = Math.max(low, Math.min(first, second));
      high = Math.min(high, Math.max(first, second));
      if (low > high) return null;
    }
  }
  return [low, high];
}

export function visibleConnector(state, connector) {
  expect(connector, 'WASM bridge must expose the painted connector line').toHaveProperty('line');
  const start = state.atoms.find((atom) => atom.index === connector.start);
  const end = state.atoms.find((atom) => atom.index === connector.end);
  const startInterval = clippedInterval(connector.line, start.sceneBounds);
  const endInterval = clippedInterval(connector.line, end.sceneBounds);
  const low = startInterval ? startInterval[1] : 0;
  const high = endInterval ? endInterval[0] : 1;
  const [a, b] = connector.line;
  const length = Math.hypot(b.x - a.x, b.y - a.y);
  const point = (fraction) =>
    Object.fromEntries(
      ['x', 'y', 'pageX', 'pageY'].map((key) => [key, a[key] + fraction * (b[key] - a[key])]),
    );
  return { low, high, length: (high - low) * length, midpoint: point((low + high) / 2) };
}

export function checkMonomerGeometry(state, atomCount, connectorCount) {
  expect(state.atoms).toHaveLength(atomCount);
  expect(state.connectors).toHaveLength(connectorCount);
  for (const [i, atom] of state.atoms.entries()) {
    expect(atom).toHaveProperty('sceneBounds');
    for (const other of state.atoms.slice(i + 1)) {
      expect(
        rectanglesOverlap(atom.sceneBounds, other.sceneBounds),
        `beads ${atom.index} (${atom.label}) and ${other.index} (${other.label}) overlap`,
      ).toBe(false);
    }
  }
  const beadHeight = Math.min(...state.atoms.map((atom) => atom.sceneBounds.height));
  for (const connector of state.connectors) {
    const visible = visibleConnector(state, connector);
    expect(visible.length, `connector ${connector.index} must have a visible gap`).toBeGreaterThan(
      beadHeight * 0.25,
    );
    for (const atom of state.atoms) {
      if ([connector.start, connector.end].includes(atom.index)) continue;
      const interval = clippedInterval(connector.line, atom.sceneBounds);
      if (interval) {
        expect(
          Math.min(interval[1], visible.high) - Math.max(interval[0], visible.low),
          `bead ${atom.index} hides connector ${connector.index}`,
        ).toBeLessThanOrEqual(0.001);
      }
    }
  }
}

export async function expectMonomerGeometry(page, atomCount, connectorCount) {
  await expect(async () =>
    checkMonomerGeometry(await scene(page), atomCount, connectorCount),
  ).toPass({ timeout: 5000 });
}

export async function expectConnectorLabels(page, connectorIndex, expectedLabels) {
  let state = await scene(page);
  const connector = state.connectors.find((item) => item.index === connectorIndex);
  const { midpoint } = visibleConnector(state, connector);
  await page.mouse.move(1100, 40);
  await page.mouse.move(midpoint.pageX, midpoint.pageY, { steps: 4 });
  await expect(async () => {
    state = await scene(page);
    expect(state, 'WASM bridge must expose visible bound AP labels').toHaveProperty(
      'attachmentLabels',
    );
    expect(state.attachmentLabels.map((label) => label.text).sort()).toEqual(
      [...expectedLabels].sort(),
    );
    for (const [i, label] of state.attachmentLabels.entries()) {
      for (const atom of state.atoms) {
        expect(
          rectanglesOverlap(label.sceneBounds, atom.sceneBounds),
          `attachment label ${label.text} overlaps bead ${atom.index}`,
        ).toBe(false);
      }
      for (const other of state.attachmentLabels.slice(i + 1)) {
        expect(
          rectanglesOverlap(label.sceneBounds, other.sceneBounds),
          `attachment labels ${label.text} and ${other.text} overlap`,
        ).toBe(false);
      }
    }
  }).toPass({ timeout: 5000 });
  await page.mouse.move(1100, 40);
}
