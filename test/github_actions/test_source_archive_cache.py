"""Tests for the optional CMake dependency source archive cache."""

import hashlib
import http.server
import json
import re
import shutil
import subprocess
import tarfile
import threading
import zipfile
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CACHE_VARIABLE = "SKETCHER_DEPENDENCY_SOURCE_CODE_CACHE_DIR"


def _configure_external(tmp_path,
                        cache_directory=None,
                        archive_hashes=None,
                        source_url_base="http://127.0.0.1:9/"):
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
        versions_file = source_directory / "versions.json"
        versions = json.loads(versions_file.read_text())
        versions["source_archive_sha256"].update(archive_hashes)
        versions_file.write_text(json.dumps(versions))
        contents = cmake_lists.read_text()
        contents = contents.replace(
            "https://gitlab.com/libeigen/eigen/-/archive/",
            source_url_base,
        )
        contents = contents.replace(
            "https://www.sqlite.org/${YEAR}/",
            source_url_base + "${YEAR}/",
        )
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


def _hash_archives(archives):
    """
    Return the SHA256 hashes of the given Eigen and SQLite archives, keyed by
    library name.
    """
    return {
        library: hashlib.sha256(archive.read_bytes()).hexdigest()
        for library, archive in zip(("eigen", "sqlite"), archives)
    }


def test_source_archive_cache_is_optional_and_resolves_relative_paths(tmp_path):
    """
    Unset paths keep CMake defaults; relative cache paths are resolved against
    the working directory.
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
    cache_root = tmp_path / "offline cache with spaces"
    build_directory, download_directories = _configure_external(
        tmp_path / "offline",
        cache_directory=str(cache_root),
        archive_hashes=_hash_archives((eigen_archive, sqlite_archive)),
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


def test_corrupt_source_cache_recovers(tmp_path):
    """
    Both real download targets replace invalid cached files.
    """
    archives = _create_test_archives(tmp_path / "fixtures")
    archives_by_name = {
        archive.name: archive.read_bytes() for archive in archives
    }

    class Handler(http.server.BaseHTTPRequestHandler):

        def do_GET(self):
            contents = archives_by_name.get(self.path.rsplit("/", 1)[-1])
            if contents is None:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Length", str(len(contents)))
            self.end_headers()
            self.wfile.write(contents)

        def log_message(self, *args):
            pass

    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            cache_root = tmp_path / "cache with spaces"
            build_directory, _ = _configure_external(
                tmp_path / "configure",
                cache_directory=str(cache_root),
                archive_hashes=_hash_archives(archives),
                source_url_base=f"http://127.0.0.1:{server.server_port}/")
            for library, archive in zip(("eigen", "sqlite"), archives):
                cached_archive = cache_root / library / archive.name
                cached_archive.write_bytes(b"corrupt cached archive")
                subprocess.run([
                    shutil.which("cmake"), "--build",
                    str(build_directory), "--target", f"{library}-download"
                ],
                               check=True,
                               capture_output=True,
                               text=True)
                assert cached_archive.read_bytes() == archive.read_bytes()
        finally:
            server.shutdown()
            thread.join()
