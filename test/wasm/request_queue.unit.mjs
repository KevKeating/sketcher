import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const shell = await readFile(new URL('../../wasm/public/wasm_shell.html', import.meta.url), 'utf8');
const script = shell.match(/<script type="text\/javascript">([\s\S]*?)<\/script>/)[1];

// Exercise the actual shell initialization without requiring a compiled WASM
// artifact. The tests control when Qt resumes by draining the queue explicitly.
async function loadShell(usesAsyncify, bindings = {}, webAssembly = WebAssembly) {
  const released = [];
  const restored = [];
  const instance = {
    sketcherUsesAsyncify: usesAsyncify,
    stackSave: () => 123,
    stackRestore: (stack) => restored.push(stack),
    getExceptionMessage: () => ['std::runtime_error', 'invalid input'],
    decrementExceptionRefcount: (error) => released.push(error),
    ...bindings,
  };
  const elements = new Map();
  const window = {};
  const context = vm.createContext({
    window,
    WebAssembly: webAssembly,
    document: {
      querySelector: (selector) => {
        if (!elements.has(selector)) {
          elements.set(selector, { style: {}, addEventListener() {} });
        }
        return elements.get(selector);
      },
    },
    qtLoad: async () => instance,
    schrodinger_sketcher_entry() {},
    console: { error: (error) => assert.fail(String(error)) },
  });
  vm.runInContext(script, context);
  await vm.runInContext('init()', context);
  assert.equal(window.Module, instance);
  return { instance, released, restored };
}

for (const usesAsyncify of [true, false]) {
  const mode = usesAsyncify ? 'Asyncify' : 'JSPI';

  test(`${mode}: identical calls preserve arguments, results, and order`, async () => {
    const calls = [];
    const format = { value: 3 };
    const helper = () => 42;
    const { instance } = await loadShell(usesAsyncify, {
      Format: { SMILES: format },
      sketcher_clear: () => calls.push('clear'),
      sketcher_import_text: (text) => calls.push(text),
      sketcher_export_text: (actualFormat) => {
        assert.equal(actualFormat, format);
        return calls.join(',');
      },
      _sketcher_test_binding: () => true,
      runtimeHelper: helper,
    });
    const { sketcher_clear, sketcher_import_text, sketcher_export_text } = instance;
    const clear = sketcher_clear();
    const imported = sketcher_import_text('CCO');
    const exported = sketcher_export_text(instance.Format.SMILES);
    assert.equal(typeof exported.then, 'function');
    assert.equal(instance.runtimeHelper, helper);
    assert.equal(instance.Format.SMILES, format);
    if (usesAsyncify) {
      assert.deepEqual(calls, []);
      instance.drainSketcherRequests();
    } else {
      assert.equal(instance.drainSketcherRequests, undefined);
      assert.deepEqual(calls, ['clear', 'CCO']);
    }
    await Promise.all([clear, imported]);
    assert.equal(await exported, 'clear,CCO');
    const inspection = instance._sketcher_test_binding();
    if (usesAsyncify) instance.drainSketcherRequests();
    assert.equal(await inspection, true);
  });

  test(`${mode}: both image overloads survive wrapping`, async () => {
    // Embind resolves overloads through the current Module property, even
    // when the original dispatcher was captured before wrapping it.
    let instance;
    const binding = (...args) => instance.get_image_bytes.overloadTable[args.length](...args);
    binding.overloadTable = {
      2: (text, format) => `${text}:${format}`,
      3: (text, format, options) => `${text}:${format}:${options.width}`,
    };
    ({ instance } = await loadShell(usesAsyncify, { get_image_bytes: binding }));
    const first = instance.get_image_bytes('CC', 'SVG');
    const second = instance.get_image_bytes('CC', 'PNG', { width: 222 });
    if (usesAsyncify) instance.drainSketcherRequests();
    assert.equal(await first, 'CC:SVG');
    assert.equal(await second, 'CC:PNG:222');
  });

  for (const native of [true, false]) {
    test(`${mode}: ${native ? 'native' : 'emulated'} C++ exceptions are released`, async () => {
      const tag = native ? new WebAssembly.Tag({ parameters: ['i32'] }) : null;
      const error = native ? new WebAssembly.Exception(tag, [17]) : 17;
      const { instance, released, restored } = await loadShell(usesAsyncify, {
        getCppExceptionTag: () => tag,
        sketcher_import_text: () => {
          throw error;
        },
        sketcher_is_empty: () => true,
      });
      const imported = instance.sketcher_import_text('invalid');
      const rejected = assert.rejects(imported, /std::runtime_error: invalid input/);
      const empty = instance.sketcher_is_empty();
      if (usesAsyncify) instance.drainSketcherRequests();
      await rejected;
      assert.equal(await empty, true);
      assert.deepEqual(released, [error]);
      assert.deepEqual(restored, [123]);
    });
  }

  test(`${mode}: JavaScript errors reject without C++ cleanup`, async () => {
    const error = new Error('bad arguments');
    const { instance, released, restored } = await loadShell(
      usesAsyncify,
      {
        sketcher_import_text: () => {
          throw error;
        },
      },
      {}, // Browsers without native WebAssembly exception support.
    );
    const rejected = assert.rejects(instance.sketcher_import_text(), (actual) => actual === error);
    if (usesAsyncify) instance.drainSketcherRequests();
    await rejected;
    assert.deepEqual(released, []);
    assert.deepEqual(restored, []);
  });
}

test('Asyncify: requests submitted during a drain wait until the next drain', async () => {
  let followup;
  let instance;
  ({ instance } = await loadShell(true, {
    sketcher_clear: () => {
      followup = instance.sketcher_is_empty();
    },
    sketcher_is_empty: () => true,
  }));
  const cleared = instance.sketcher_clear();
  instance.drainSketcherRequests();
  await cleared;
  let finished = false;
  followup.then(() => {
    finished = true;
  });
  await Promise.resolve();
  assert.equal(finished, false);
  instance.drainSketcherRequests();
  assert.equal(await followup, true);
});
