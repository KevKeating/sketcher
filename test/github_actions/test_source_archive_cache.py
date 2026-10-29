"""Tests for the pinned source archive cache used by the external build action."""

import hashlib
import importlib.util
import json
import shutil
import subprocess
import tarfile
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
HELPER_PATH = REPOSITORY_ROOT / ".github/actions/build-external/source_archive_cache.py"
SPEC = importlib.util.spec_from_file_location("source_archive_cache",
                                              HELPER_PATH)
SOURCE_ARCHIVE_CACHE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SOURCE_ARCHIVE_CACHE)


def configure_external(tmp_path, build_type="Debug", eigen_version=None):
    """
    Configure a disposable copy and record its ExternalProject properties.
    """
    cmake = shutil.which("cmake")
    assert cmake is not None
    tmp_path.mkdir(parents=True)

    source_dir = tmp_path / "external"
    shutil.copytree(REPOSITORY_ROOT / "external", source_dir)
    if eigen_version:
        versions_path = source_dir / "versions.json"
        versions = json.loads(versions_path.read_text())
        versions["eigen"] = eigen_version
        versions_path.write_text(json.dumps(versions))

    properties_file = tmp_path / "external-properties.txt"
    with (source_dir / "CMakeLists.txt").open("a",
                                              encoding="utf-8") as cmake_file:
        cmake_file.write(f"""
foreach(LIBRARY eigen sqlite)
  ExternalProject_Get_Property(${{LIBRARY}} URL URL_HASH DOWNLOAD_DIR DOWNLOAD_NAME)
  file(RELATIVE_PATH CACHE_DIRECTORY ${{CMAKE_BINARY_DIR}} ${{DOWNLOAD_DIR}})
  file(TO_CMAKE_PATH "${{CACHE_DIRECTORY}}" CACHE_DIRECTORY)
  string(SHA256 SOURCE_KEY "v1|${{LIBRARY}}|${{URL}}|${{URL_HASH}}")
  file(APPEND "{properties_file.as_posix()}"
       "${{LIBRARY}}|${{URL}}|${{URL_HASH}}|${{CACHE_DIRECTORY}}|${{DOWNLOAD_NAME}}|${{SOURCE_KEY}}\\n")
endforeach()
""")

    build_dir = tmp_path / "build"
    subprocess.run(
        [
            cmake,
            "-S",
            str(source_dir),
            "-B",
            str(build_dir),
            f"-DCMAKE_BUILD_TYPE={build_type}",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    metadata = json.loads((build_dir / "dependency-archives.json").read_text())
    properties = {}
    for line in properties_file.read_text().splitlines():
        library, url, url_hash, directory, archive, key = line.split("|", 5)
        properties[library] = {
            "url": url,
            "url_hash": url_hash,
            "directory": directory,
            "archive": archive,
            "key": key,
        }
    return metadata, properties


def test_external_cmake_emits_stable_archive_metadata(tmp_path):
    """
    The real declarations emit relative metadata and source-only keys.
    """
    debug_metadata, debug_properties = configure_external(tmp_path / "debug")
    release_metadata, release_properties = configure_external(
        tmp_path / "release", build_type="Release")

    assert debug_properties == release_properties
    assert debug_metadata == release_metadata
    assert set(debug_metadata["archives"]) == {"eigen", "sqlite"}
    for library, properties in debug_properties.items():
        entry = debug_metadata["archives"][library]
        assert entry["directory"] == f"downloads/{library}"
        assert entry["archive"] == properties["archive"]
        assert entry["sha256"] == properties["url_hash"].removeprefix("SHA256=")
        assert entry["key"] == properties["key"]
        assert len(entry["sha256"]) == 64
        assert len(entry["key"]) == 64
    assert "https://gitlab.com/libeigen/eigen/" in debug_properties["eigen"][
        "url"]
    assert all("https://www.sqlite.org/" in url
               for url in debug_properties["sqlite"]["url"].split(";"))


def test_source_key_changes_with_source_identity_only(tmp_path):
    """
    Changing Eigen's source identity changes only its cache key.
    """
    original_metadata, _ = configure_external(tmp_path / "original")
    changed_metadata, _ = configure_external(
        tmp_path / "changed", eigen_version="source-cache-test-version")

    original_archives = original_metadata["archives"]
    changed_archives = changed_metadata["archives"]
    assert original_archives["eigen"]["key"] != changed_archives["eigen"]["key"]
    assert original_archives["sqlite"]["key"] == changed_archives["sqlite"][
        "key"]


def test_archive_cache_helper_accepts_only_the_pinned_archive(tmp_path):
    """
    The action helper accepts only an archive matching its declared hash.
    """
    archive_data = b"fixture source archive"
    digest = hashlib.sha256(archive_data).hexdigest()
    metadata = {
        "archives": {
            "eigen": {
                "directory": "downloads/eigen",
                "archive": "eigen.tar.gz",
                "sha256": digest,
                "key": hashlib.sha256(b"source identity").hexdigest(),
            }
        }
    }
    (tmp_path / "dependency-archives.json").write_text(json.dumps(metadata))
    entry, archive_path = SOURCE_ARCHIVE_CACHE.get_archive(tmp_path, "eigen")
    archive_path.parent.mkdir(parents=True)
    archive_path.write_bytes(archive_data)

    assert SOURCE_ARCHIVE_CACHE.is_valid_archive(archive_path, entry["sha256"])
    archive_path.write_bytes(b"partial archive")
    assert not SOURCE_ARCHIVE_CACHE.is_valid_archive(archive_path,
                                                     entry["sha256"])
    archive_path.unlink()
    assert not SOURCE_ARCHIVE_CACHE.is_valid_archive(archive_path,
                                                     entry["sha256"])


def test_cmake_reuses_a_restored_archive_without_network(tmp_path):
    """
    An existing validated DOWNLOAD_DIR archive is sufficient for CMake.
    """
    cmake = shutil.which("cmake")
    assert cmake is not None

    source_archive = tmp_path / "source.tar.gz"
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\nproject(archive_fixture NONE)\n")
    with tarfile.open(source_archive, "w:gz") as archive:
        archive.add(source_dir / "CMakeLists.txt",
                    arcname="fixture/CMakeLists.txt")
    archive_hash = hashlib.sha256(source_archive.read_bytes()).hexdigest()

    fixture_cmake = tmp_path / "fixture-project"
    fixture_cmake.mkdir()
    (fixture_cmake /
     "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.25)
project(archive_cache_test NONE)
include(ExternalProject)
ExternalProject_Add(sample
  URL \"${ARCHIVE_URL}\"
  URL_HASH SHA256=${ARCHIVE_HASH}
  DOWNLOAD_NAME source.tar.gz
  DOWNLOAD_DIR \"${CMAKE_BINARY_DIR}/downloads/sample\"
  TIMEOUT 5
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  CONFIGURE_COMMAND \"\"
  BUILD_COMMAND \"\"
  INSTALL_COMMAND \"\")
""")
    build_dir = tmp_path / "build"
    download_dir = build_dir / "downloads/sample"
    download_dir.mkdir(parents=True)
    shutil.copyfile(source_archive, download_dir / "source.tar.gz")
    source_archive.unlink()

    subprocess.run(
        [
            cmake,
            "-S",
            str(fixture_cmake),
            "-B",
            str(build_dir),
            "-DARCHIVE_URL=http://127.0.0.1:9/source.tar.gz",
            f"-DARCHIVE_HASH={archive_hash}",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run(
        [cmake, "--build",
         str(build_dir), "--target", "sample"],
        check=True,
        capture_output=True,
        text=True,
    )

    assert sorted(
        path.name for path in download_dir.iterdir()) == ["source.tar.gz"]
    assert (build_dir / "sample-prefix/src/sample/CMakeLists.txt").is_file()
