# Dependency source caching

The external project can still be configured and built normally:

```sh
cmake -S external -B build/external -G Ninja
cmake --build build/external --target eigen
```

To reuse pristine dependency sources between build directories, enable the
optional local cache. This requires Python 3.12 or newer and Ninja on PATH:

```sh
cmake -S external -B build/external -G Ninja \
    -DSKETCHER_EXTERNAL_SOURCE_CACHE=/absolute/path/to/source-cache
cmake --build build/external --target eigen
```

With caching enabled, a missing source snapshot is fetched through a separate,
download-only CMake project. Each failed download step is retried up to four
times with approximately 15, 30, and 60 seconds of backoff plus jitter, within a
15-minute deadline. Each attempt includes CMake's existing immediate clone
retries. Recognized authentication and missing-ref errors fail immediately.
Patching and compilation are not retried. Without the cache option, the normal
ExternalProject download and update behavior remains in effect.

Each snapshot contains an unpatched source checkout, including Git metadata and
initialized submodules, in `source.tar.gz`. The adjacent `source.json` records
its source options, checksum, and top-level Git revision when applicable. Every
build extracts its own writable source tree and applies its current patches.
The cached path never becomes the build's source directory. CMake download and
update stamps, configured build trees, and installed libraries are not included.

Cache keys have the form `external-source-v1-<name>-<source identity hash>`.
The hash includes the repository/tag or download URLs/checksum, and the Git fetch
and submodule options. It excludes branch names of Sketcher, patches, build
settings, and installed-library versions other than the source's own tag or URL.
Different versions coexist. `v1` identifies the snapshot format and compatibility
rules; bump it when existing snapshots become incompatible. Tags are assumed
immutable: changing a remote tag will not refresh an existing snapshot. Remove
the corresponding entry explicitly if its upstream contents must be refreshed.
Invalid or incomplete snapshots fail validation; remove them before retrying.

`sketcher_external_project_add` in `source_cache.cmake` forwards the existing
declarations and generates `source-manifest.json` and download-only CMake
projects in the build directory. Extend its source-option list and tests when
introducing a new download option. Source snapshots can have a superset of a
build's submodules: Qt's host and target builds share the snapshot containing
`qtbase` and `qtsvg`, while the host build still builds only `qtbase`.

## GitHub Actions

The formatting/test job generates the preparation matrix directly from CMake:

```sh
cmake -S external -B build/source-metadata -G Ninja \
    -DSKETCHER_EXTERNAL_SOURCES_ONLY=ON
python3 external/source_cache.py outputs \
    --manifest build/source-metadata/source-manifest.json \
    --cache-dir build/source-cache
```

`SKETCHER_EXTERNAL_SOURCES_ONLY` allows metadata generation without a compiler.
Each preparation job checks one exact source-cache key without downloading its
contents. On a miss, it fetches the sources once, saves the snapshot, and uploads
a one-day workflow artifact. All preparation and build jobs check out the exact
Sketcher commit used to generate the matrix. Artifact names include a unique
preparation prefix, so different invocations of the reusable workflow in the
same nightly run cannot collide.

The six builders check their installed-library caches first. On a binary miss,
they restore only the required source snapshot, with cross-OS caching enabled,
or download the preparation artifact. If neither is available, the local cache
helper fetches the sources with retries. Preparation and storage failures do not
prevent the use of existing installed-library caches. Concurrent independent
workflow runs may both fetch a cold entry; there is no distributed lock.

GitHub's branch access restrictions and eviction policy still apply. In
particular, sibling branches cannot freely share caches, while default-branch
caches can be reused by other branches. Priming commonly used versions from a
workflow on the default branch improves reuse. The source snapshots cover the
projects declared here; additional downloads performed inside upstream builds
remain the responsibility of those builds.

Run the helper tests with CMake, Ninja, Git, and pytest installed:

```sh
pytest test/github_actions/
```

The integration tests use disposable local Git repositories and a local HTTP
server, including offline restoration, submodule relocation, patch application,
version changes, URL fallback, and corrupt archives. They do not download the
real dependencies.
