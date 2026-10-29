"""Tests for the optional CMake dependency source archive cache."""

import hashlib
import json
import re
import shutil
import subprocess
import tarfile
import zipfile
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CACHE_VARIABLE = "SKETCHER_DEPENDENCY_SOURCE_CODE_CACHE_DIR"


def _configure_external(tmp_path, cache_directory=None, archive_hashes=None):
    """
    Configure a disposable copy and return its build directory properties.
    """
    cmake = shutil.which("cmake")
    assert cmake is not None
    tmp_path.mkdir(parents=True)
    source_directory = tmp_path / "external"
    shutil.copytree(REPOSITORY_ROOT / "external", source_directory)
    cmake_lists = source_directory / "CMakeLists.txt"

    if archive_hashes:
        contents = cmake_lists.read_text()
        contents = contents.replace(
            "https://gitlab.com/libeigen/eigen/-/archive/",
            "http://127.0.0.1:9/",
        )
        contents = contents.replace(
            "https://www.sqlite.org/${YEAR}/",
            "http://127.0.0.1:9/${YEAR}/",
        )
        contents, count = re.subn(
            r"(URL_HASH\s+SHA256=)[0-9a-fA-F]+",
            lambda match: match.group(1) + next(archive_hashes),
            contents,
        )
        assert count == 2
        contents = contents.replace(
            "DOWNLOAD_NAME eigen-${EIGEN_VERSION}.tar.bz2",
            "DOWNLOAD_NAME eigen-${EIGEN_VERSION}.tar.bz2\n  TIMEOUT 1",
        )
        contents = contents.replace(
            "DOWNLOAD_NAME ${SQLITE_URLS_ARCHIVE_NAME}",
            "DOWNLOAD_NAME ${SQLITE_URLS_ARCHIVE_NAME}\n  TIMEOUT 1",
        )
        cmake_lists.write_text(contents)

    properties_file = tmp_path / "download-directories.txt"
    with cmake_lists.open("a", encoding="utf-8") as cmake_file:
        cmake_file.write(f"""
foreach(LIBRARY eigen sqlite)
  ExternalProject_Get_Property(${{LIBRARY}} DOWNLOAD_DIR)
  file(APPEND "{properties_file.as_posix()}"
       "${{LIBRARY}}|${{DOWNLOAD_DIR}}\\n")
endforeach()
ExternalProject_Add_StepTargets(eigen download)
ExternalProject_Add_StepTargets(sqlite download)
""")

    build_directory = tmp_path / "build"
    configure_command = [
        cmake,
        "-S",
        str(source_directory),
        "-B",
        str(build_directory),
    ]
    if cache_directory is not None:
        configure_command.append(f"-D{CACHE_VARIABLE}={cache_directory}")
    subprocess.run(configure_command,
                   check=True,
                   capture_output=True,
                   text=True,
                   cwd=tmp_path)

    download_directories = {}
    for line in properties_file.read_text().splitlines():
        library, directory = line.split("|", 1)
        download_directories[library] = directory
    return build_directory, download_directories


def _create_test_archives(directory):
    """
    Create small, valid archives that can be extracted by ExternalProject.
    """
    directory.mkdir(parents=True)
    source_file = directory / "fixture.txt"
    source_file.write_text("offline source archive fixture")
    versions = json.loads(
        (REPOSITORY_ROOT / "external/versions.json").read_text())
    eigen_archive_name = f"eigen-{versions['eigen']}.tar.bz2"
    sqlite_digits = re.sub(r"[^0-9]", "", versions["sqlite"]).ljust(7, "0")

    eigen_archive = directory / eigen_archive_name
    with tarfile.open(eigen_archive, "w:bz2") as archive:
        archive.add(source_file, arcname="eigen-fixture/fixture.txt")

    sqlite_archive = directory / f"sqlite-amalgamation-{sqlite_digits}.zip"
    with zipfile.ZipFile(sqlite_archive, "w") as archive:
        archive.write(source_file, "sqlite-fixture/fixture.txt")

    return eigen_archive, sqlite_archive


def test_source_archive_cache_is_optional_and_resolves_relative_paths(tmp_path):
    """
    Unset paths keep CMake defaults; relative cache paths become absolute.
    """
    default_build, default_directories = _configure_external(tmp_path /
                                                             "default")
    assert {
        library: Path(path) for library, path in default_directories.items()
    } == {
        library: default_build / f"{library}-prefix/src"
        for library in ("eigen", "sqlite")
    }
    assert not (default_build / "dependency-archives.json").exists()

    relative_cache_directory = "source cache with spaces"
    cache_build, cache_directories = _configure_external(
        tmp_path / "configured", cache_directory=relative_cache_directory)
    expected_root = (tmp_path / "configured" /
                     relative_cache_directory).resolve()
    assert all(
        Path(cache_directories[library]) == expected_root / library
        for library in ("eigen", "sqlite"))
    assert all(Path(path).is_dir() for path in cache_directories.values())
    assert not (cache_build / "dependency-archives.json").exists()


def test_eigen_and_sqlite_download_steps_reuse_cached_archives_offline(
        tmp_path):
    """
    Real ExternalProject download targets reuse hash-pinned fixture archives.
    """
    cmake = shutil.which("cmake")
    assert cmake is not None
    fixture_directory = tmp_path / "fixtures"
    eigen_archive, sqlite_archive = _create_test_archives(fixture_directory)
    archive_hashes = iter(
        hashlib.sha256(archive.read_bytes()).hexdigest()
        for archive in (eigen_archive, sqlite_archive))
    cache_root = tmp_path / "offline cache with spaces"
    build_directory, download_directories = _configure_external(
        tmp_path / "offline",
        cache_directory=str(cache_root),
        archive_hashes=archive_hashes,
    )

    for library, archive in (
        ("eigen", eigen_archive),
        ("sqlite", sqlite_archive),
    ):
        destination = cache_root / library / archive.name
        shutil.copyfile(archive, destination)
        subprocess.run(
            [
                cmake, "--build",
                str(build_directory), "--target", f"{library}-download"
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        assert destination.stat().st_size == archive.stat().st_size
        assert Path(download_directories[library]) == cache_root / library
