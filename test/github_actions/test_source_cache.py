"""Exercise real CMake download steps against local, disposable Git remotes."""
import importlib.util
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import threading

import pytest

REPO = Path(__file__).resolve().parents[2]
HELPER = REPO / "external" / "source_cache.cmake"
SPEC = importlib.util.spec_from_file_location("source_cache",
                                              HELPER.with_suffix(".py"))
source_cache = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source_cache)


def run(*command, cwd=None):
    result = subprocess.run(command,
                            cwd=cwd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True)
    assert result.returncode == 0, result.stdout
    return result.stdout


@pytest.fixture(autouse=True)
def isolated_git(tmp_path, monkeypatch):
    # Prevent user settings, credentials and network access from affecting tests.
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", str(tmp_path / "gitconfig"))
    monkeypatch.setenv("GIT_ALLOW_PROTOCOL", "file")


@pytest.fixture
def cmake():
    executable = shutil.which("cmake")
    if executable is None or shutil.which("ninja") is None:
        pytest.fail("Source cache integration tests require CMake and Ninja")
    return executable


def git(directory, *args):
    return run("git", "-C", str(directory), *args)


def commit(directory, message):
    git(directory, "add", ".")
    git(directory, "-c", "user.name=Source Cache Test", "-c",
        "user.email=source-cache@example.invalid", "-c", "commit.gpgsign=false",
        "commit", "-m", message)


def repository(directory):
    directory.mkdir(parents=True)
    run("git", "init", str(directory))
    (directory / "payload.txt").write_text("original\n",
                                           encoding="utf-8",
                                           newline="\n")
    commit(directory, "Initial source")
    git(directory, "tag", "v1")
    return directory


def repository_with_submodule(directory):
    child = repository(directory / "child")
    parent = repository(directory / "parent")
    git(parent, "-c", "protocol.file.allow=always", "submodule", "add",
        child.as_uri(), "module")
    commit(parent, "Add submodule")
    git(parent, "tag", "with-module")
    return parent


def configure(cmake, directory, declarations, *, cache=None):
    project = directory / "project"
    project.mkdir(parents=True, exist_ok=True)
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\nproject(cache_test NONE)\n"
        f"include(ExternalProject)\ninclude([=[{HELPER.as_posix()}]=])\n" +
        declarations,
        encoding="utf-8")
    build = directory / "build"
    run(cmake, "-S", str(project), "-B", str(build), "-G", "Ninja",
        f"-DPython3_EXECUTABLE={sys.executable}",
        f"-DSKETCHER_EXTERNAL_SOURCE_CACHE={cache or ''}")
    manifest = json.loads((build / "source-manifest.json").read_text())
    return build, manifest


def declaration(remote, tag="v1", patch="", *, submodules=""):
    return ("sketcher_external_project_add(sample\n"
            f" GIT_REPOSITORY [=[{remote.as_uri()}]=] GIT_TAG {tag}\n"
            f" GIT_SHALLOW TRUE {submodules}\n{patch}\n"
            ' CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")\n')


def build_target(cmake, build, target):
    return run(cmake, "--build", str(build), "--target", target)


def test_cached_git_sources_patch_and_relocate_offline(tmp_path, cmake):
    remote = repository(tmp_path / "remote")
    cache = tmp_path / "cache"
    patch = tmp_path / "change.patch"
    patch.write_text(
        "diff --git a/payload.txt b/payload.txt\n"
        "--- a/payload.txt\n+++ b/payload.txt\n"
        "@@ -1 +1 @@\n-original\n+patched\n",
        encoding="utf-8",
        newline="\n")
    declarations = declaration(
        remote,
        patch=f"PATCH_COMMAND git reset --hard HEAD && git apply "
        f"[=[{patch.as_posix()}]=]")
    first_build, manifest = configure(cmake,
                                      tmp_path / "first",
                                      declarations,
                                      cache=cache)
    archive = source_cache.fetch(manifest["sample"], cache, cmake=cmake)
    with tarfile.open(archive) as snapshot:
        assert snapshot.extractfile(
            "source/payload.txt").read() == b"original\n"
        assert "source/.git/HEAD" in snapshot.getnames()
        assert not any("-stamp/" in name for name in snapshot.getnames())
    remote.rename(tmp_path / "unreachable-remote")
    for build in [
            first_build,
            configure(cmake, tmp_path / "second", declarations, cache=cache)[0]
    ]:
        build_target(cmake, build, "sample")
        source = build / "sample-prefix" / "src" / "sample"
        assert (source / "payload.txt").read_text() == "patched\n"
        assert git(source, "show", "HEAD:payload.txt") == "original\n"
        assert "patched" in git(source, "diff")
        # Reconfiguration must not re-enable the native Git updater.
        run(cmake, "-S", str(build.parent / "project"), "-B", str(build))
        build_target(cmake, build, "sample")
    assert source_cache.archive_digest(archive) == json.loads(
        (archive.parent / "source.json").read_text())["sha256"]


def test_submodule_git_metadata_survives_relocation(tmp_path, cmake):
    origins = tmp_path / "origins"
    parent = repository_with_submodule(origins)
    cache = tmp_path / "cache"
    build, manifest = configure(cmake,
                                tmp_path / "consumer",
                                declaration(parent, "with-module"),
                                cache=cache)
    source_cache.fetch(manifest["sample"], cache, cmake=cmake)
    origins.rename(tmp_path / "unreachable-origins")
    build_target(cmake, build, "sample")
    module = build / "sample-prefix" / "src" / "sample" / "module"
    assert git(module, "show", "HEAD:payload.txt") == "original\n"
    (module / "payload.txt").write_text("changed\n")
    git(module, "reset", "--hard", "HEAD")
    assert (module / "payload.txt").read_text() == "original\n"


def test_versions_coexist_and_switch_in_same_build_tree(tmp_path, cmake):
    remote = repository(tmp_path / "remote")
    (remote / "payload.txt").write_text("version two\n")
    commit(remote, "Second version")
    git(remote, "tag", "v2")
    cache = tmp_path / "cache"
    keys = set()
    for tag, expected in [("v1", "original\n"), ("v2", "version two\n"),
                          ("v1", "original\n")]:
        build, manifest = configure(cmake,
                                    tmp_path / "consumer",
                                    declaration(remote, tag),
                                    cache=cache)
        keys.add(manifest["sample"]["key"])
        build_target(cmake, build, "sample")
        source = build / "sample-prefix" / "src" / "sample"
        assert (source / "payload.txt").read_text() == expected
    assert len(keys) == 2
    assert all((cache / key / "source.tar.gz").is_file() for key in keys)


def test_uncached_external_project_retains_native_download(tmp_path, cmake):
    remote = repository(tmp_path / "remote")
    build, _ = configure(cmake, tmp_path / "consumer", declaration(remote))
    build_target(cmake, build, "sample")
    source = build / "sample-prefix" / "src" / "sample"
    assert git(source, "show", "HEAD:payload.txt") == "original\n"


def test_url_fallback_and_pristine_archive(tmp_path, cmake):
    upstream = tmp_path / "upstream.tar.gz"
    with tarfile.open(upstream, "w:gz") as archive:
        data = b"sqlite source\n"
        info = tarfile.TarInfo("upstream/sqlite.c")
        info.size = len(data)
        info.mode = 0o755
        archive.addfile(info, io.BytesIO(data))
    # CMake requires HTTP URLs for a multi-URL fallback list.
    handler = partial(SimpleHTTPRequestHandler, directory=str(tmp_path))
    with ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}"
            options = f"URL {url}/missing.tar.gz {url}/upstream.tar.gz"
            build, manifest = configure(
                cmake,
                tmp_path / "consumer", "sketcher_external_project_add(sqlite "
                f"{options} CONFIGURE_COMMAND \"\" BUILD_COMMAND \"\" INSTALL_COMMAND \"\")",
                cache=tmp_path / "cache")
            source_cache.fetch(manifest["sqlite"],
                               tmp_path / "cache",
                               cmake=cmake)
        finally:
            server.shutdown()
            thread.join()
    upstream.unlink()
    build_target(cmake, build, "sqlite")
    source = build / "sqlite-prefix" / "src" / "sqlite"
    assert (source / "sqlite.c").read_bytes() == b"sqlite source\n"


@pytest.mark.parametrize("cached", [False, True])
def test_explicit_cache_submodules_preserve_native_selection(
        tmp_path, cmake, cached):
    parent = repository_with_submodule(tmp_path / "origins")
    cache = tmp_path / "cache" if cached else None
    build, manifest = configure(
        cmake,
        tmp_path / "consumer",
        declaration(parent,
                    "with-module",
                    submodules='GIT_SUBMODULES "" '
                    'SKETCHER_SOURCE_CACHE_GIT_SUBMODULES module'),
        cache=cache)
    _, baseline = configure(
        cmake, tmp_path / "baseline",
        declaration(parent, "with-module", submodules="GIT_SUBMODULES module"))
    # Making the override explicit must not invalidate existing source snapshots.
    assert manifest["sample"]["key"] == baseline["sample"]["key"]
    build_target(cmake, build, "sample")
    source = build / "sample-prefix" / "src" / "sample"
    assert (source / "module" / "payload.txt").is_file() == cached


def test_project_manifest_is_stable_across_host_builds(tmp_path, cmake):
    manifests = []
    for host in ["OFF", "ON"]:
        build = tmp_path / host
        run(cmake, "-S", str(REPO / "external"), "-B", str(build), "-G",
            "Ninja", "-DSKETCHER_EXTERNAL_SOURCES_ONLY=ON",
            f"-DQT_HOST_BUILD={host}")
        manifests.append(
            json.loads((build / "source-manifest.json").read_text()))
    assert len(manifests[0]) == 9
    assert {
        name: entry["key"] for name, entry in manifests[0].items()
    } == {
        name: entry["key"] for name, entry in manifests[1].items()
    }
    assert "qtbase" in manifests[0]["qt"]["options"]
    assert "qtsvg" in manifests[0]["qt"]["options"]


def test_build_and_patch_options_do_not_invalidate_sources(tmp_path, cmake):
    remote = repository(tmp_path / "remote")
    keys = []
    for patch in ["", "PATCH_COMMAND ${CMAKE_COMMAND} -E echo changed"]:
        _, manifest = configure(cmake, tmp_path / "consumer",
                                declaration(remote, patch=patch))
        keys.append(manifest["sample"]["key"])
    assert keys[0] == keys[1]


def test_empty_submodule_selection_is_preserved(tmp_path, cmake):
    parent = repository(tmp_path / "parent")
    # The snapshot must not attempt to fetch this deliberately unavailable URL.
    (parent / ".gitmodules").write_text(
        '[submodule "unavailable"]\n\tpath = module\n'
        '\turl = https://unavailable.invalid/repository.git\n',
        newline="\n")
    commit(parent, "Describe optional module")
    git(parent, "update-index", "--add", "--cacheinfo",
        "160000," + git(parent, "rev-parse", "HEAD").strip() + ",module")
    git(parent, "-c", "user.name=Source Cache Test", "-c",
        "user.email=source-cache@example.invalid", "-c", "commit.gpgsign=false",
        "commit", "-m", "Record optional module")
    git(parent, "tag", "with-module")
    cache = tmp_path / "cache"
    _, manifest = configure(cmake,
                            tmp_path / "consumer",
                            declaration(parent,
                                        "with-module",
                                        submodules='GIT_SUBMODULES ""'),
                            cache=cache)
    assert manifest["sample"]["options"][-2:] == ["GIT_SUBMODULES", ""]
    source_cache.fetch(manifest["sample"], cache, cmake=cmake)


def test_failed_fetch_publishes_no_partial_snapshot(tmp_path, cmake,
                                                    monkeypatch):
    remote = repository(tmp_path / "remote")
    _, manifest = configure(cmake, tmp_path / "consumer", declaration(remote))

    def fail(command):
        raise subprocess.CalledProcessError(1, command)

    monkeypatch.setattr(source_cache, "download_with_retry", fail)
    cache = tmp_path / "cache"
    with pytest.raises(subprocess.CalledProcessError):
        source_cache.fetch(manifest["sample"], cache, cmake=cmake)
    assert list(cache.iterdir()) == []


def test_workflow_outputs_and_local_snapshot_reuse(tmp_path, cmake):
    remote = repository(tmp_path / "remote")
    build, manifest = configure(cmake, tmp_path / "consumer",
                                declaration(remote))
    script = str(HELPER.with_suffix(".py"))
    common = [
        "--manifest",
        str(build / "source-manifest.json"), "--cache-dir",
        str(tmp_path / "cache")
    ]
    outputs = []
    for _ in range(2):
        output = run(sys.executable, script, "outputs", *common, cwd=REPO)
        outputs.append(dict(line.split("=", 1) for line in output.splitlines()))
    entry = json.loads(outputs[0]["source-matrix"])["include"][0]
    assert entry["key"] == manifest["sample"]["key"]
    assert outputs[0]["source-ref"] == git(REPO, "rev-parse", "HEAD").strip()
    assert outputs[0]["source-artifact-prefix"] != outputs[1][
        "source-artifact-prefix"]
    for expected in ["available=false", "available=true"]:
        output = run(sys.executable, script, "entry-outputs", *common,
                     "--library", "sample")
        assert expected in output.splitlines()
        source_cache.fetch(manifest["sample"], tmp_path / "cache", cmake=cmake)


def test_corrupt_snapshot_is_rejected_without_fetch(tmp_path, cmake,
                                                    monkeypatch):
    remote = repository(tmp_path / "remote")
    _, manifest = configure(cmake, tmp_path / "consumer", declaration(remote))
    entry = manifest["sample"]
    archive = source_cache.fetch(entry, tmp_path / "cache", cmake=cmake)
    archive.write_bytes(b"corrupt")
    monkeypatch.setattr(
        source_cache.subprocess, "run",
        lambda *a, **k: pytest.fail("Unexpected upstream access"))
    with pytest.raises(ValueError, match="Invalid source snapshot"):
        source_cache.fetch(entry, tmp_path / "cache", cmake=cmake)


def test_load_errors_retry_with_exponential_backoff(monkeypatch):
    attempts = []
    sleeps = []

    def download(command, **kwargs):
        attempts.append(command)
        failed = len(attempts) < 4
        return subprocess.CompletedProcess(
            command, 1 if failed else 0, "GitLab is currently unable to handle "
            "this request due to load\n")

    monkeypatch.setattr(source_cache.subprocess, "run", download)
    monkeypatch.setattr(source_cache.time, "sleep", sleeps.append)
    monkeypatch.setattr(source_cache.random, "uniform", lambda *args: 0)
    source_cache.download_with_retry(["download"])
    assert len(attempts) == 4
    assert sleeps == [15, 30, 60]


@pytest.mark.parametrize("message,expected_attempts", [
    ("GitLab is currently unable to handle this request due to load", 4),
    ("fatal: couldn't find remote ref nonexistent", 1),
    ("fatal: Authentication failed", 1),
])
def test_retry_failures_are_bounded(monkeypatch, message, expected_attempts):
    attempts = []

    def download(command, **kwargs):
        attempts.append(command)
        return subprocess.CompletedProcess(command, 1, message)

    monkeypatch.setattr(source_cache.subprocess, "run", download)
    monkeypatch.setattr(source_cache.time, "sleep", lambda delay: None)
    with pytest.raises(subprocess.CalledProcessError):
        source_cache.download_with_retry(["download"])
    assert len(attempts) == expected_attempts


def test_retry_deadline_prevents_another_attempt(monkeypatch):
    monkeypatch.setattr(
        source_cache.subprocess, "run",
        lambda command, **kwargs: subprocess.CompletedProcess(
            command, 1, "overloaded"))
    monkeypatch.setattr(source_cache.time, "monotonic", lambda: 0)
    monkeypatch.setattr(
        source_cache.time, "sleep",
        lambda delay: pytest.fail("Must not sleep beyond the deadline"))
    with pytest.raises(TimeoutError, match="deadline"):
        source_cache.download_with_retry(["download"], timeout=1)
