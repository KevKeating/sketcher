"""Read and validate source archive cache metadata from external CMake."""

import hashlib
import json
import os
import re
import sys
from pathlib import Path


def get_archive(build_dir: Path, library: str):
    """
    Return cache metadata and archive path for one configured dependency.
    """
    metadata_path = build_dir / "dependency-archives.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    entry = metadata["archives"][library]
    directory = Path(entry["directory"])
    archive = entry["archive"]
    key = entry["key"]
    sha256 = entry["sha256"]
    if directory.is_absolute() or ".." in directory.parts:
        raise ValueError(
            "archive directory must be relative to the build directory")
    if Path(archive).name != archive:
        raise ValueError("archive must be a file name")
    if not re.fullmatch(r"[0-9a-f]{64}", key):
        raise ValueError("archive cache key must be a SHA256 digest")
    if not re.fullmatch(r"[0-9a-f]{64}", sha256):
        raise ValueError("archive hash must be a SHA256 digest")
    return entry, build_dir / directory / archive


def is_valid_archive(path: Path, sha256: str) -> bool:
    """
    Check that a downloaded archive is complete and matches its pin.
    """
    if not path.is_file() or path.stat().st_size == 0:
        return False
    digest = hashlib.sha256()
    with path.open("rb") as archive_file:
        for chunk in iter(lambda: archive_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest() == sha256


def write_output(**values):
    output_path = os.environ.get("GITHUB_OUTPUT")
    if output_path:
        with open(output_path, "a", encoding="utf-8") as output_file:
            for name, value in values.items():
                output_file.write(f"{name}={value}\n")


def main() -> int:
    mode, build_dir, library = sys.argv[1:4]
    try:
        entry, archive_path = get_archive(Path(build_dir), library)
    except (KeyError, OSError, TypeError, ValueError, json.JSONDecodeError):
        write_output(path="", key="", valid="false")
        return 0

    if mode == "prepare":
        write_output(path=archive_path.as_posix(), key=entry["key"], valid="")
    elif mode == "check":
        write_output(
            path=archive_path.as_posix(),
            key=entry["key"],
            valid=str(is_valid_archive(archive_path, entry["sha256"])).lower(),
        )
    else:
        raise ValueError(f"unknown mode: {mode}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
