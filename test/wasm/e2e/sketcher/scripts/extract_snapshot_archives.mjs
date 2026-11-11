import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync } from 'node:fs';
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
  if (!existsSync(archiveRoot)) return;
  mkdirSync(snapshotRoot, { recursive: true });
  const archives = readdirSync(archiveRoot).filter((name) => name.endsWith('.tar.gz'));
  for (const archive of archives.sort()) {
    extractTarGz(path.join(archiveRoot, archive), snapshotRoot);
  }
}
