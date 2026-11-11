import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, renameSync, rmSync } from 'node:fs';
import path from 'node:path';
import { archiveRoot, e2eDir, snapshotRoot, tarPath } from './extract_snapshot_archives.mjs';

// These make an archive depend only on the names and contents of its
// snapshots, so repacking unchanged snapshots on any machine reproduces the
// committed archive byte for byte.  They need GNU tar.
const DETERMINISTIC_TAR_OPTIONS = [
  '--format=gnu',
  '--sort=name',
  '--mtime=@0',
  '--owner=0',
  '--group=0',
  '--numeric-owner',
  // Store files as 644 and directories as 755 whatever the local umask (CI has
  // produced 640 and 750) and drop setgid bits inherited from a parent
  // directory.
  '--mode=u=rwX,go=rX,ug-s',
];

function isGnuTar(command) {
  try {
    return execFileSync(command, ['--version'], { encoding: 'utf8' }).includes('GNU tar');
  } catch {
    return false;
  }
}

/**
 * Find GNU tar and the environment to run it in.  On Windows, PowerShell and
 * cmd find Windows' own tar.exe, which is bsdtar and has no --sort, so fall
 * back to the GNU tar that comes with Git for Windows.  That tar runs gzip, so
 * Git's usr/bin goes on the PATH too.
 */
function findGnuTar() {
  if (isGnuTar('tar')) return { command: 'tar', env: process.env };
  if (process.platform === 'win32') {
    // git --exec-path is <Git>/<mingw64 or ucrt64>/libexec/git-core
    const gitExecPath = execFileSync('git', ['--exec-path'], { encoding: 'utf8' }).trim();
    const usrBin = path.resolve(gitExecPath, '..', '..', '..', 'usr', 'bin');
    const command = path.join(usrBin, 'tar.exe');
    if (isGnuTar(command)) {
      const pathKey =
        Object.keys(process.env).find((key) => key.toUpperCase() === 'PATH') ?? 'PATH';
      const env = {
        ...process.env,
        [pathKey]: `${usrBin}${path.delimiter}${process.env[pathKey] ?? ''}`,
      };
      return { command, env };
    }
  }
  throw new Error(
    'Archiving snapshots needs GNU tar.  On Windows, install Git for Windows or run this from Git Bash.',
  );
}

if (!existsSync(snapshotRoot)) {
  throw new Error(`No extracted Playwright snapshots found at ${snapshotRoot}`);
}

function testExists(testName) {
  return existsSync(path.join(e2eDir, testName));
}

// Only archive snapshots of test files that still exist, so that leftovers of
// deleted or renamed tests don't come back as new archives.
const snapshots = [];
for (const entry of readdirSync(snapshotRoot, { withFileTypes: true })) {
  if (!entry.isDirectory()) continue;
  if (testExists(entry.name)) {
    snapshots.push(entry.name);
  } else {
    console.warn(`Skipping ${entry.name} snapshots: there is no e2e/${entry.name} test`);
  }
}
snapshots.sort();

const tar = findGnuTar();
mkdirSync(archiveRoot, { recursive: true });
for (const name of snapshots) {
  // A separate deterministic archive per test keeps PR changes localized.
  const archivePath = path.join(archiveRoot, `${name}.tar.gz`);
  const temporaryPath = `${archivePath}.tmp`;
  execFileSync(
    tar.command,
    [
      ...DETERMINISTIC_TAR_OPTIONS,
      '-czf',
      tarPath(e2eDir, temporaryPath),
      '-C',
      tarPath(e2eDir, snapshotRoot),
      name,
    ],
    { cwd: e2eDir, env: tar.env },
  );
  renameSync(temporaryPath, archivePath);
}

// Remove an archive only when its test is gone.  A test without a snapshot
// directory may just not have run, or may have failed before its first
// screenshot, after SKETCHER_REGENERATE_SNAPSHOTS started from empty.
for (const entry of readdirSync(archiveRoot)) {
  if (entry.endsWith('.tar.gz') && !testExists(entry.slice(0, -'.tar.gz'.length))) {
    rmSync(path.join(archiveRoot, entry));
  }
}
