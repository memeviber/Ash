#!/usr/bin/env python3
"""Rename Basalt source files from the legacy extension to .bsl.

The legacy suffix is assembled rather than written literally so this script
remains usable after the repository-wide migration has completed.
"""
from __future__ import annotations

import argparse
from pathlib import Path

OLD_EXT = "." + "bas" + "alt"
NEW_EXT = ".bsl"
SKIP_DIRS = {".git", ".tmp", "_build", "__pycache__"}
TEXT_SUFFIXES = {
    ".bsl", ".c", ".h", ".ml", ".mli", ".md", ".sh", ".py", ".json",
    ".toml", ".yaml", ".yml", ".txt", ".sha256", ".dune", "",
}


def iter_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP_DIRS for part in path.relative_to(root).parts):
            continue
        yield path


def rename_target(path: Path) -> Path:
    name = path.name
    if name.endswith(OLD_EXT + ".c"):
        return path.with_name(name[: -len(OLD_EXT + ".c")] + NEW_EXT + ".c")
    if name.endswith(OLD_EXT):
        return path.with_name(name[: -len(OLD_EXT)] + NEW_EXT)
    return path


def source_text(path: Path) -> bool:
    return path.suffix in TEXT_SUFFIXES or path.name.endswith(".sha256")


def main() -> int:
    parser = argparse.ArgumentParser(description="Rename Basalt source files to .bsl")
    parser.add_argument("root", nargs="?", default=".", help="repository root")
    parser.add_argument("--check", action="store_true", help="report changes without modifying files")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    files = list(iter_files(root))
    renames = [(path, rename_target(path)) for path in files if rename_target(path) != path]
    destinations = {dst for _, dst in renames}
    conflicts = [dst for dst in destinations if dst.exists() and dst not in {src for src, _ in renames}]
    if conflicts:
        for path in conflicts:
            print(f"error: rename destination already exists: {path}")
        return 2

    for src, dst in sorted(renames):
        print(f"rename {src.relative_to(root)} -> {dst.relative_to(root)}")
    if args.check:
        return 0

    for src, dst in sorted(renames, reverse=True):
        src.rename(dst)

    for path in iter_files(root):
        if not source_text(path):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        updated = text.replace(OLD_EXT + ".c", NEW_EXT + ".c").replace(OLD_EXT, NEW_EXT)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
