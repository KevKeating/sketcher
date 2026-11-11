import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, rmSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const sketcherDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const e2eDir = path.resolve(sketcherDir, '..');
export const archiveRoot = path.join(e2eDir, 'reference_archives');
export const snapshotRoot = path.join(e2eDir, '__snapshots__');

/**
 * Return `target` relative to `cwd`, with forward slashes, for passing to a tar
 * process that runs in `cwd`.  tar is never given absolute paths because Git
 * Bash's GNU tar reads the drive letter of a Windows path ("F:\...") as the
 * name of a remote host.
 */
export function tarPath(cwd, target) {
  return path.relative(cwd, target).split(path.sep).join('/');
}

export function extractTarGz(archivePath, outputDir) {
  execFileSync('tar', ['-xzf', tarPath(outputDir, archivePath)], { cwd: outputDir });
}

export default function extractSnapshotArchives() {
  if (process.env.SKETCHER_REGENERATE_SNAPSHOTS === '1') {
    // Start without any snapshots, so that a full run with
    // --update-snapshots=all writes exactly the snapshots that the tests take,
    // and repacking drops the ones that no test takes anymore.
    if (process.env.PLAYWRIGHT_SKIP_SCREENSHOTS === '1') {
      throw new Error(
        'SKETCHER_REGENERATE_SNAPSHOTS needs the screenshots that PLAYWRIGHT_SKIP_SCREENSHOTS skips',
      );
    }
    rmSync(snapshotRoot, { force: true, recursive: true });
    mkdirSync(snapshotRoot, { recursive: true });
    return;
  }
  if (!existsSync(archiveRoot)) return;
  mkdirSync(snapshotRoot, { recursive: true });
  const archives = readdirSync(archiveRoot).filter((name) => name.endsWith('.tar.gz'));
  for (const archive of archives.sort()) {
    // Replace the test's snapshot directory rather than extracting over it, so
    // that it holds exactly the archive's snapshots.  Otherwise snapshots that
    // the archive no longer has (for example, ones extracted on another branch)
    // would stay behind, and the next repack would put them back.  Local
    // snapshot changes that haven't been repacked are lost either way.
    const testName = archive.slice(0, -'.tar.gz'.length);
    rmSync(path.join(snapshotRoot, testName), { force: true, recursive: true });
    extractTarGz(path.join(archiveRoot, archive), snapshotRoot);
  }
}
