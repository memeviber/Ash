#!/usr/bin/env python3
"""Basalt package manager MVP.

The tool is intentionally a repository-side helper while package imports are
still being specified in the Bootstrap compiler. It never executes package
scripts and never builds the frozen Host compiler.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - Python 3.11 is required by CI.
    tomllib = None  # type: ignore[assignment]


NAME_RE = re.compile(r"^[a-z][a-z0-9-]{0,63}$")
VERSION_RE = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")


class PackageError(Exception):
    """A user-facing package-manager error."""


@dataclass(frozen=True)
class Version:
    major: int
    minor: int
    patch: int
    prerelease: tuple[str, ...] = ()

    @classmethod
    def parse(cls, value: str) -> "Version":
        match = VERSION_RE.fullmatch(value)
        if match is None:
            raise PackageError(f"invalid SemVer: {value!r}")
        pre = tuple(match.group(4).split(".")) if match.group(4) else ()
        if any(item.isdigit() and len(item) > 1 and item.startswith("0") for item in pre):
            raise PackageError(f"invalid SemVer: {value!r}")
        return cls(int(match.group(1)), int(match.group(2)), int(match.group(3)), pre)

    def __str__(self) -> str:
        suffix = "-" + ".".join(self.prerelease) if self.prerelease else ""
        return f"{self.major}.{self.minor}.{self.patch}{suffix}"

    def _key(self) -> tuple[Any, ...]:
        if not self.prerelease:
            return (self.major, self.minor, self.patch, 1)
        parts: list[tuple[int, Any]] = []
        for item in self.prerelease:
            if item.isdigit():
                parts.append((0, int(item)))
            else:
                parts.append((1, item))
        return (self.major, self.minor, self.patch, 0, tuple(parts))

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key() < other._key()

    def __le__(self, other: object) -> bool:
        return self == other or self < other

    def __gt__(self, other: object) -> bool:
        return not self <= other

    def __ge__(self, other: object) -> bool:
        return not self < other


@dataclass(frozen=True)
class Constraint:
    operator: str
    version: Version | None


def _parse_partial_version(value: str) -> tuple[int, int, int, int]:
    bits = value.split(".")
    if not 1 <= len(bits) <= 3 or any(not bit.isdigit() for bit in bits):
        raise PackageError(f"invalid version requirement: {value!r}")
    nums = [int(bit) for bit in bits]
    while len(nums) < 3:
        nums.append(0)
    return nums[0], nums[1], nums[2], len(bits)


def parse_requirement(value: str) -> list[Constraint]:
    value = value.strip()
    if not value:
        raise PackageError("empty version requirement")
    if value in {"*", "x", "X"}:
        return []
    constraints: list[Constraint] = []
    for raw_term in value.split(","):
        term = raw_term.strip()
        if not term:
            raise PackageError(f"invalid empty term in requirement {value!r}")
        if term.startswith("^"):
            base = Version.parse(_normalize_version(term[1:]))
            if base.major > 0:
                upper = Version(base.major + 1, 0, 0)
            elif base.minor > 0:
                upper = Version(0, base.minor + 1, 0)
            else:
                upper = Version(0, 0, base.patch + 1)
            constraints += [Constraint(">=", base), Constraint("<", upper)]
            continue
        if term.startswith("~"):
            base = Version.parse(_normalize_version(term[1:]))
            constraints += [Constraint(">=", base), Constraint("<", Version(base.major, base.minor + 1, 0))]
            continue
        operator = "="
        for candidate in (">=", "<=", ">", "<", "="):
            if term.startswith(candidate):
                operator = candidate
                term = term[len(candidate):].strip()
                break
        if "x" in term.lower() or "*" in term:
            bits = re.split(r"[.]", term)
            if len(bits) > 3:
                raise PackageError(f"invalid wildcard requirement: {value!r}")
            fixed: list[int] = []
            for bit in bits:
                if bit.lower() in {"x", "*"}:
                    break
                if not bit.isdigit():
                    raise PackageError(f"invalid wildcard requirement: {value!r}")
                fixed.append(int(bit))
            if not fixed:
                continue
            lower = Version(*(fixed + [0] * (3 - len(fixed))))
            if len(fixed) == 1:
                upper = Version(lower.major + 1, 0, 0)
            else:
                upper = Version(lower.major, lower.minor + 1, 0)
            constraints += [Constraint(">=", lower), Constraint("<", upper)]
            continue
        if "-" in term:
            constraints.append(Constraint(operator, Version.parse(_normalize_version(term))))
            continue
        partial = _parse_partial_version(term)
        if partial[3] != 3 and operator == "=":
            lower = Version(partial[0], partial[1], partial[2])
            upper = Version(partial[0] + 1, 0, 0) if partial[3] == 1 else Version(partial[0], partial[1] + 1, 0)
            constraints += [Constraint(">=", lower), Constraint("<", upper)]
        else:
            constraints.append(Constraint(operator, Version.parse(_normalize_version(term))))
    return constraints


def _normalize_version(value: str) -> str:
    parts = value.split(".")
    if len(parts) > 3:
        return value
    numeric = parts[:]
    prerelease = ""
    if "-" in numeric[-1]:
        numeric[-1], prerelease = numeric[-1].split("-", 1)
        prerelease = "-" + prerelease
    while len(numeric) < 3:
        numeric.append("0")
    return ".".join(numeric) + prerelease


def satisfies(version: Version, requirement: str) -> bool:
    for constraint in parse_requirement(requirement):
        if constraint.version is None:
            continue
        if constraint.operator == "=" and version != constraint.version:
            return False
        if constraint.operator == ">=" and version < constraint.version:
            return False
        if constraint.operator == ">" and version <= constraint.version:
            return False
        if constraint.operator == "<=" and version > constraint.version:
            return False
        if constraint.operator == "<" and version >= constraint.version:
            return False
    if version.prerelease and "-" not in requirement:
        return False
    return True


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_toml(path: Path) -> dict[str, Any]:
    if tomllib is None:
        raise PackageError("Python 3.11 or newer is required")
    try:
        with path.open("rb") as stream:
            return tomllib.load(stream)
    except FileNotFoundError as exc:
        raise PackageError(f"missing file: {path}") from exc
    except tomllib.TOMLDecodeError as exc:
        raise PackageError(f"invalid TOML in {path}: {exc}") from exc


def validate_name(name: str) -> str:
    if NAME_RE.fullmatch(name) is None:
        raise PackageError(f"invalid package name: {name!r}")
    return name


def load_manifest(root: Path) -> dict[str, Any]:
    path = root / "Basalt.toml"
    data = load_toml(path)
    package = data.get("package")
    if not isinstance(package, dict):
        raise PackageError("Basalt.toml requires a [package] table")
    raw_name = package.get("name")
    raw_version = package.get("version")
    raw_entry = package.get("entry", "src/main.basalt")
    if not isinstance(raw_name, str) or not isinstance(raw_version, str) or not isinstance(raw_entry, str):
        raise PackageError("package name, version, and entry must be strings")
    name = validate_name(raw_name)
    version = Version.parse(raw_version)
    entry = raw_entry
    entry_path = (root / entry).resolve()
    if entry_path != root.resolve() and root.resolve() not in entry_path.parents:
        raise PackageError("package entry must remain inside package root")
    if not entry.endswith(".basalt"):
        raise PackageError("package entry must have the .basalt extension")
    if not entry_path.exists():
        raise PackageError(f"package entry does not exist: {entry}")
    return {"raw": data, "name": name, "version": version, "entry": entry, "entry_path": entry_path}


def dependency_spec(value: Any) -> tuple[str, dict[str, Any]]:
    if isinstance(value, str):
        return value, {}
    if isinstance(value, dict):
        unknown = set(value) - {"version", "registry", "path"}
        if unknown:
            raise PackageError(f"dependency has unsupported keys: {sorted(unknown)}")
        requirement = value.get("version", "*")
        if not isinstance(requirement, str):
            raise PackageError("dependency version must be a string")
        path = value.get("path")
        registry = value.get("registry")
        if path is not None and (not isinstance(path, str) or not path or "\x00" in path):
            raise PackageError("dependency path must be a non-empty string")
        if registry is not None and (not isinstance(registry, str) or not registry or "\x00" in registry):
            raise PackageError("dependency registry must be a non-empty string")
        if path is not None and registry is not None:
            raise PackageError("dependency cannot specify both path and registry")
        return requirement, dict(value)
    raise PackageError("dependency must be a version string or inline table")


def validate_archive_reference(archive: Any, context: str) -> str:
    if not isinstance(archive, str) or not archive or archive.startswith("/") or "\\" in archive:
        raise PackageError(f"{context} has an invalid archive path")
    parsed_archive = urllib.parse.urlparse(archive)
    archive_parts = archive.split("/")
    if (
        parsed_archive.scheme
        or parsed_archive.netloc
        or parsed_archive.query
        or parsed_archive.fragment
        or any(part in {"", ".", ".."} for part in archive_parts)
    ):
        raise PackageError(f"{context} has an invalid archive path")
    return archive


def registry_path(registry: str, suffix: str) -> tuple[str, Path | None]:
    parsed = urllib.parse.urlparse(registry)
    if parsed.scheme in {"", "file"}:
        if parsed.scheme == "file" and parsed.netloc not in {"", "localhost"}:
            raise PackageError("file registry must not contain a remote host")
        base = Path(urllib.request.url2pathname(parsed.path) if parsed.scheme == "file" else registry)
        return "", base / suffix
    if parsed.scheme != "https" or not parsed.netloc or parsed.username or parsed.password:
        raise PackageError("registry must be a local directory, file:// URL, or https:// URL")
    return urllib.parse.urljoin(registry.rstrip("/") + "/", suffix), None


def read_registry_index(registry: str, name: str) -> list[dict[str, Any]]:
    target, local_path = registry_path(registry, f"index/{name}.json")
    try:
        if local_path is not None:
            text = local_path.read_text(encoding="utf-8")
        else:
            with urllib.request.urlopen(target, timeout=20) as response:
                text = response.read().decode("utf-8")
        data = json.loads(text)
    except (OSError, ValueError, urllib.error.URLError) as exc:
        raise PackageError(f"cannot read registry index for {name}: {exc}") from exc
    if not isinstance(data, list):
        raise PackageError(f"registry index for {name} must be an array")
    result: list[dict[str, Any]] = []
    seen_versions: set[Version] = set()
    for item in data:
        if not isinstance(item, dict):
            raise PackageError(f"registry index for {name} contains a non-object record")
        record = dict(item)
        raw_name = record.get("name")
        raw_version = record.get("version")
        if not isinstance(raw_name, str) or validate_name(raw_name) != name:
            raise PackageError(f"registry index record has wrong name for {name}")
        if not isinstance(raw_version, str):
            raise PackageError(f"registry index record for {name} has a non-string version")
        version = Version.parse(raw_version)
        if version in seen_versions:
            raise PackageError(f"registry index for {name} contains duplicate version {version}")
        seen_versions.add(version)
        checksum = record.get("checksum")
        if not isinstance(checksum, str) or not SHA256_RE.fullmatch(checksum):
            raise PackageError(f"registry record {name} {version} lacks a valid SHA-256 checksum")
        archive = validate_archive_reference(record.get("archive"), f"registry record {name} {version}")
        dependencies = record.get("dependencies", {})
        if not isinstance(dependencies, dict):
            raise PackageError(f"registry dependencies for {name} {version} must be a table")
        normalized_dependencies: dict[str, Any] = {}
        for child_name, child_value in sorted(dependencies.items()):
            child = validate_name(str(child_name)) if isinstance(child_name, str) else ""
            if not child:
                raise PackageError(f"registry dependencies for {name} {version} contain an invalid name")
            _requirement, normalized_spec = dependency_spec(child_value)
            normalized_dependencies[child] = normalized_spec if normalized_spec else child_value
        record["checksum"] = checksum
        record["archive"] = archive
        record["dependencies"] = normalized_dependencies
        record["parsed_version"] = version
        result.append(record)
    return result


def _version_string(value: Any) -> str:
    return str(value)


@dataclass
class ResolvedPackage:
    name: str
    version: Version
    source: str
    checksum: str
    dependencies: list[str]
    registry: str = ""
    archive: str = ""
    path: Path | None = None
    dependency_specs: dict[str, Any] | None = None

    @property
    def identity(self) -> str:
        return f"{self.name} {self.version}"


def lock_records(root: Path) -> dict[str, dict[str, Any]]:
    lock_path = root / "Basalt.lock"
    if not lock_path.exists():
        return {}
    data = load_toml(lock_path)
    if data.get("format") != 1:
        raise PackageError("Basalt.lock format must be 1")
    records = data.get("package", [])
    if not isinstance(records, list):
        raise PackageError("Basalt.lock package entries must be an array")
    result: dict[str, dict[str, Any]] = {}
    for record in records:
        if not isinstance(record, dict):
            raise PackageError("Basalt.lock contains an invalid package record")
        name = validate_name(str(record.get("name", "")))
        if name in result:
            raise PackageError(f"duplicate package record in Basalt.lock: {name}")
        version = Version.parse(str(record.get("version", "")))
        source = str(record.get("source", ""))
        if source.startswith("registry+"):
            checksum = str(record.get("checksum", ""))
            if not SHA256_RE.fullmatch(checksum):
                raise PackageError(f"invalid checksum in lockfile for {name}")
            archive = validate_archive_reference(record.get("archive"), f"registry lock record {name}")
            record["archive"] = archive
        elif source.startswith("path+"):
            if not source.removeprefix("path+"):
                raise PackageError(f"path lock record lacks path for {name}")
        else:
            raise PackageError(f"unsupported lockfile source for {name}: {source}")
        dependencies = record.get("dependencies", [])
        if not isinstance(dependencies, list) or any(not isinstance(item, str) for item in dependencies):
            raise PackageError(f"invalid dependency edge list for {name}")
        normalized_dependencies = [validate_name(item) for item in dependencies]
        if len(set(normalized_dependencies)) != len(normalized_dependencies):
            raise PackageError(f"duplicate dependency edge for {name}")
        result[name] = {
            **record,
            "name": name,
            "version": str(version),
            "dependencies": sorted(normalized_dependencies),
        }
    for name, record in result.items():
        for dependency in record["dependencies"]:
            if dependency not in result:
                raise PackageError(f"lockfile edge {name} -> {dependency} has no package record")
    ordered = sorted(result, key=lambda item: (item, Version.parse(result[item]["version"])))
    if list(result) != ordered:
        raise PackageError("Basalt.lock package records must be sorted by name and version")
    return result


def resolve(root: Path, update: bool = False, registry_override: str = "") -> tuple[dict[str, Any], list[ResolvedPackage]]:
    manifest = load_manifest(root)
    raw = manifest["raw"]
    dependencies = raw.get("dependencies", {})
    if not isinstance(dependencies, dict):
        raise PackageError("[dependencies] must be a table")
    locked = {} if update else lock_records(root)
    base_requirements: dict[str, list[str]] = {}
    base_specs: dict[str, list[dict[str, Any]]] = {}
    base_roots: dict[str, list[Path]] = {}
    for name_value, dependency_value in sorted(dependencies.items()):
        name = validate_name(str(name_value))
        requirement, spec = dependency_spec(dependency_value)
        base_requirements.setdefault(name, []).append(requirement)
        base_specs.setdefault(name, []).append(spec)
        base_roots.setdefault(name, []).append(root)

    def package_key(package: ResolvedPackage) -> tuple[Any, ...]:
        return (package.name, package.version, package.source, package.checksum, package.archive, package.path)

    last_failure: list[str] = []

    def choose_candidates(name: str, requirements: list[str], specs: list[dict[str, Any]], dependency_roots: list[Path]) -> list[ResolvedPackage]:
        path_specs = [spec for spec in specs if spec.get("path") is not None]
        if path_specs:
            if len(path_specs) != len(specs):
                last_failure[:] = [f"conflicting registry and path sources for {name}"]
                return []
            path_values = {spec.get("path") for spec in path_specs}
            if len(path_values) != 1 or not isinstance(next(iter(path_values)), str):
                last_failure[:] = [f"conflicting path sources for {name}"]
                return []
            path_value = str(next(iter(path_values)))
            path_targets = {(base / path_value).resolve() for base in dependency_roots}
            if len(path_targets) != 1:
                last_failure[:] = [f"conflicting path sources for {name}"]
                return []
            dep_root = next(iter(path_targets))
            if not dep_root.is_dir():
                last_failure[:] = [f"path dependency does not exist: {next(iter(path_values))}"]
                return []
            dep_manifest = load_manifest(dep_root)
            if dep_manifest["name"] != name:
                last_failure[:] = [f"path dependency name mismatch: expected {name}, got {dep_manifest['name']}"]
                return []
            if not all(satisfies(dep_manifest["version"], requirement) for requirement in requirements):
                last_failure[:] = [f"no path version of {name} satisfies all requirements: {requirements!r}"]
                return []
            child_deps = dep_manifest["raw"].get("dependencies", {})
            if not isinstance(child_deps, dict):
                raise PackageError(f"[dependencies] in {name} must be a table")
            source = "path+" + os.path.relpath(dep_root, root).replace(os.sep, "/")
            return [ResolvedPackage(
                name,
                dep_manifest["version"],
                source,
                "",
                sorted(validate_name(str(child)) for child in child_deps),
                path=dep_root,
                dependency_specs=dict(child_deps),
            )]

        registries = {str(spec.get("registry", registry_override)) for spec in specs}
        if "" in registries:
            last_failure[:] = [f"dependency {name} requires --registry or BASALT_REGISTRY"]
            return []
        if len(registries) != 1:
            last_failure[:] = [f"conflicting registries for {name}"]
            return []
        registry = next(iter(registries))
        candidates = read_registry_index(registry, name)
        locked_version = locked.get(name, {}).get("version")
        if locked_version is not None:
            locked_parsed = Version.parse(str(locked_version))
            valid = [candidate for candidate in candidates if candidate["parsed_version"] == locked_parsed]
            if not valid:
                raise PackageError(f"locked version {name} {locked_version} is not present in the registry")
        else:
            valid = candidates
        valid = [candidate for candidate in valid if all(satisfies(candidate["parsed_version"], requirement) for requirement in requirements)]
        if not valid:
            last_failure[:] = [f"no registry version of {name} satisfies all requirements: {requirements!r}"]
            return []
        resolved_candidates: list[ResolvedPackage] = []
        for selected in sorted(valid, key=lambda candidate: candidate["parsed_version"], reverse=True):
            version = selected["parsed_version"]
            deps = selected.get("dependencies", {})
            if not isinstance(deps, dict):
                raise PackageError(f"registry dependencies for {name} {version} must be a table")
            child_names = sorted(validate_name(str(child)) for child in deps)
            if any(isinstance(value, dict) and value.get("path") is not None for value in deps.values()):
                last_failure[:] = [f"registry package {name} {version} cannot declare a path dependency"]
                continue
            resolved_candidates.append(
                ResolvedPackage(
                    name,
                    version,
                    "registry+" + registry,
                    str(selected["checksum"]),
                    child_names,
                    registry=registry,
                    archive=str(selected["archive"]),
                    dependency_specs=dict(deps),
                )
            )
        return resolved_candidates

    def closure(current: dict[str, ResolvedPackage]) -> set[str]:
        needed = set(base_requirements)
        pending = list(sorted(needed))
        while pending:
            name = pending.pop(0)
            package = current.get(name)
            if package is None:
                continue
            for child in package.dependencies:
                if child not in needed:
                    needed.add(child)
                    pending.append(child)
        return needed

    def state_key(current: dict[str, ResolvedPackage]) -> tuple[tuple[str, tuple[Any, ...]], ...]:
        return tuple(sorted((name, package_key(package)) for name, package in current.items()))

    visited: set[tuple[tuple[str, tuple[Any, ...]], ...]] = set()

    def search(current: dict[str, ResolvedPackage]) -> dict[str, ResolvedPackage] | None:
        state = state_key(current)
        if state in visited:
            return None
        visited.add(state)
        needed = closure(current)
        current = {name: current[name] for name in needed if name in current}
        requirements = {name: list(values) for name, values in base_requirements.items()}
        specs = {name: list(values) for name, values in base_specs.items()}
        roots = {name: list(values) for name, values in base_roots.items()}
        for name in sorted(needed):
            package = current.get(name)
            if package is None:
                continue
            parent_root = package.path if package.path is not None else root
            for child_name, child_value in sorted((package.dependency_specs or {}).items()):
                child = validate_name(str(child_name))
                child_requirement, child_spec = dependency_spec(child_value)
                if child_spec.get("path") is not None and package.path is None:
                    raise PackageError(f"registry package {package.identity} cannot declare a path dependency: {child}")
                requirements.setdefault(child, []).append(child_requirement)
                specs.setdefault(child, []).append(child_spec)
                roots.setdefault(child, []).append(parent_root)
        names = sorted(requirements)
        candidates = {
            name: choose_candidates(name, requirements[name], specs[name], roots[name])
            for name in names
        }
        empty = [name for name in names if not candidates[name]]
        if empty:
            alternatives = [
                name
                for name in names
                if name in current and len(candidates[name]) > 1
            ]
            if not alternatives:
                return None
            name = alternatives[0]
            options = [
                candidate
                for candidate in candidates[name]
                if package_key(candidate) != package_key(current[name])
            ]
        else:
            unresolved = [
                name
                for name in names
                if name not in current
                or package_key(current[name]) not in {package_key(candidate) for candidate in candidates[name]}
            ]
            if not unresolved and set(current) == needed:
                return {name: current[name] for name in sorted(needed)}
            if unresolved:
                name = unresolved[0]
                options = candidates[name]
            else:
                alternatives = [
                    name
                    for name in names
                    if name in current and len(candidates[name]) > 1
                ]
                if not alternatives:
                    return None
                name = alternatives[0]
                options = [
                    candidate
                    for candidate in candidates[name]
                    if package_key(candidate) != package_key(current[name])
                ]
        for candidate in options:
            next_current = dict(current)
            next_current[name] = candidate
            result = search(next_current)
            if result is not None:
                return result
        return None

    solved = search({})
    if solved is None:
        raise PackageError(last_failure[0] if last_failure else "dependency resolution failed")
    return manifest, sorted(solved.values(), key=lambda item: (item.name, item.version))


def resolve_offline(root: Path) -> tuple[dict[str, Any], list[ResolvedPackage]]:
    manifest = load_manifest(root)
    lock_path = root / "Basalt.lock"
    records = lock_records(root)
    if not lock_path.exists():
        raise PackageError("Basalt.lock is missing; run fetch first")
    root_value = load_toml(root / "Basalt.lock").get("root")
    expected_root = f"{manifest['name']} {manifest['version']}"
    if root_value != expected_root:
        raise PackageError(f"lockfile root mismatch: expected {expected_root!r}")
    dependencies = manifest["raw"].get("dependencies", {})
    if not isinstance(dependencies, dict):
        raise PackageError("[dependencies] must be a table")
    for name_value, dependency_value in sorted(dependencies.items()):
        name = validate_name(str(name_value))
        requirement, spec = dependency_spec(dependency_value)
        record = records.get(name)
        if record is None:
            raise PackageError(f"manifest dependency {name} is missing from Basalt.lock")
        version = Version.parse(record["version"])
        if not satisfies(version, requirement):
            raise PackageError(f"locked version {name} {version} does not satisfy {requirement}")
        source = str(record["source"])
        if "path" in spec:
            dep_root = (root / str(spec["path"])).resolve()
            expected_source = "path+" + os.path.relpath(dep_root, root).replace(os.sep, "/")
            if source != expected_source:
                raise PackageError(f"lockfile source mismatch for path dependency {name}")
            dep_manifest = load_manifest(dep_root)
            if dep_manifest["name"] != name or dep_manifest["version"] != version:
                raise PackageError(f"path dependency manifest mismatch for {name}")
        else:
            registry = str(spec.get("registry", ""))
            if registry and source != "registry+" + registry:
                raise PackageError(f"lockfile registry mismatch for {name}")
            if not source.startswith("registry+"):
                raise PackageError(f"lockfile source mismatch for registry dependency {name}")
    packages: list[ResolvedPackage] = []
    for name, record in records.items():
        source = str(record["source"])
        version = Version.parse(record["version"])
        dependencies = list(record["dependencies"])
        if source.startswith("path+"):
            dep_root = (root / source.removeprefix("path+")).resolve()
            package = ResolvedPackage(name, version, source, "", dependencies, path=dep_root)
        else:
            package = ResolvedPackage(
                name,
                version,
                source,
                str(record["checksum"]),
                dependencies,
                registry=source.removeprefix("registry+"),
                archive=str(record["archive"]),
            )
        packages.append(package)
    return manifest, sorted(packages, key=lambda item: (item.name, item.version))


def toml_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def write_lock(root: Path, manifest: dict[str, Any], packages: Iterable[ResolvedPackage]) -> None:
    lines = ["format = 1", f"root = {toml_string(manifest['name'] + ' ' + str(manifest['version']))}", ""]
    for package in packages:
        lines += ["[[package]]", f"name = {toml_string(package.name)}", f"version = {toml_string(str(package.version))}", f"source = {toml_string(package.source)}"]
        if package.checksum:
            lines.append(f"checksum = {toml_string(package.checksum)}")
            lines.append(f"archive = {toml_string(package.archive)}")
        dependencies = ", ".join(toml_string(name) for name in sorted(package.dependencies))
        lines.append(f"dependencies = [{dependencies}]")
        lines.append("")
    (root / "Basalt.lock").write_text("\n".join(lines), encoding="utf-8")


def cache_root() -> Path:
    return Path(os.environ.get("BASALT_HOME", str(Path.home() / ".basalt"))) / "cache"


def cache_path(checksum: str) -> Path:
    digest = checksum.removeprefix("sha256:")
    return cache_root() / "sha256" / digest[:2] / f"{digest}.tar.gz"


def download_archive(package: ResolvedPackage, offline: bool = False) -> Path:
    destination = cache_path(package.checksum)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and sha256_file(destination) == package.checksum.removeprefix("sha256:"):
        return destination
    if destination.exists():
        if offline:
            raise PackageError(f"cached archive checksum mismatch for {package.identity}")
        destination.unlink()
    if offline:
        raise PackageError(f"archive is not cached for {package.identity}")
    target, local_path = registry_path(package.registry, package.archive)
    temporary = destination.with_suffix(".tmp")
    try:
        if local_path is not None:
            shutil.copyfile(local_path, temporary)
        else:
            with urllib.request.urlopen(target, timeout=60) as response, temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
        actual = sha256_file(temporary)
        expected = package.checksum.removeprefix("sha256:")
        if actual != expected:
            raise PackageError(f"checksum mismatch for {package.identity}: expected {expected}, got {actual}")
        temporary.replace(destination)
    except PackageError:
        temporary.unlink(missing_ok=True)
        raise
    except (OSError, urllib.error.URLError) as exc:
        temporary.unlink(missing_ok=True)
        raise PackageError(f"cannot download {package.identity}: {exc}") from exc
    return destination


def safe_copy_local(source: Path, destination: Path) -> None:
    source = source.resolve()
    for entry in source.rglob("*"):
        if entry.is_symlink():
            raise PackageError(f"path dependency contains a symlink: {entry}")
        resolved = entry.resolve()
        if resolved != source and source not in resolved.parents:
            raise PackageError(f"path dependency escapes its root: {entry}")
    shutil.copytree(source, destination, symlinks=False)


def safe_extract(archive: Path, destination: Path, expected_top: str) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    seen_names: set[str] = set()
    with tarfile.open(archive, "r:gz") as stream:
        for member in stream.getmembers():
            name = member.name.replace("\\", "/")
            if name in seen_names:
                raise PackageError(f"duplicate archive member: {member.name}")
            seen_names.add(name)
            if name.startswith("/") or name == ".." or name.startswith("../") or "/../" in name:
                raise PackageError(f"archive path traversal: {member.name}")
            parts = Path(name).parts
            if not parts or parts[0] != expected_top:
                raise PackageError(f"archive member must be under {expected_top}/: {member.name}")
            if member.issym() or member.islnk() or not (member.isdir() or member.isfile()):
                raise PackageError(f"unsupported archive member type: {member.name}")
            target = (destination / name).resolve()
            if destination.resolve() not in target.parents and target != destination.resolve():
                raise PackageError(f"archive escapes destination: {member.name}")
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                source = stream.extractfile(member)
                if source is None:
                    raise PackageError(f"cannot read archive member: {member.name}")
                with target.open("wb") as output:
                    shutil.copyfileobj(source, output)
    extracted_root = destination / expected_top
    if not (extracted_root / "Basalt.toml").is_file():
        raise PackageError(f"package archive lacks {expected_top}/Basalt.toml")


def materialize(package: ResolvedPackage, root: Path, offline: bool = False) -> Path:
    project_root = root.resolve()
    vendor_root = (project_root / ".basalt" / "vendor").resolve()
    if project_root != vendor_root and project_root not in vendor_root.parents:
        raise PackageError("vendor directory must remain inside project root")
    vendor = vendor_root / package.name / str(package.version)
    vendor.parent.mkdir(parents=True, exist_ok=True)
    temporary_parent = vendor.parent / f".{package.version}.tmp"
    shutil.rmtree(temporary_parent, ignore_errors=True)
    if package.path is not None:
        source = package.path.resolve()
        temporary_root = temporary_parent.resolve()
        if source == project_root or source in project_root.parents or source in temporary_root.parents:
            raise PackageError(f"path dependency would recursively copy project data: {package.identity}")
        safe_copy_local(source, temporary_parent)
    else:
        archive = download_archive(package, offline=offline)
        with tempfile.TemporaryDirectory(prefix="basalt-extract-", dir=str(vendor.parent)) as temp_dir:
            temp_root = Path(temp_dir)
            safe_extract(archive, temp_root, f"{package.name}-{package.version}")
            extracted_root = temp_root / f"{package.name}-{package.version}"
            archive_manifest = load_manifest(extracted_root)
            if archive_manifest["name"] != package.name or archive_manifest["version"] != package.version:
                raise PackageError(f"archive manifest identity mismatch for {package.identity}")
            if package.dependency_specs is not None:
                expected_dependencies = package.dependency_specs
                actual_dependencies = archive_manifest["raw"].get("dependencies", {})
                if actual_dependencies != expected_dependencies:
                    raise PackageError(f"archive manifest dependencies mismatch for {package.identity}")
            shutil.copytree(extracted_root, temporary_parent, symlinks=False)
    shutil.rmtree(vendor, ignore_errors=True)
    temporary_parent.replace(vendor)
    return vendor


def cmd_init(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    manifest_path = root / "Basalt.toml"
    if manifest_path.exists() and not args.force:
        raise PackageError(f"refusing to overwrite {manifest_path}; use --force")
    name = validate_name(args.name or root.name.lower().replace("_", "-"))
    manifest_path.write_text(
        "[package]\n"
        f"name = {toml_string(name)}\n"
        "version = \"0.1.0\"\n"
        "edition = \"2026\"\n"
        "entry = \"src/main.basalt\"\n\n"
        "[dependencies]\n",
        encoding="utf-8",
    )
    entry = root / "src/main.basalt"
    if not entry.exists():
        entry.parent.mkdir(parents=True, exist_ok=True)
        entry.write_text("func main(): int {\n  return 0;\n}\n", encoding="utf-8")
    print(f"initialized package {name} in {root}")
    return 0


def _dependency_line(name: str, requirement: str) -> str:
    validate_name(name)
    if not requirement or "\n" in requirement:
        raise PackageError("dependency requirement must be a non-empty single line")
    return f"{name} = {toml_string(requirement)}"


def cmd_add(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    manifest_path = root / "Basalt.toml"
    load_manifest(root)
    if "@" in args.dependency:
        name, requirement = args.dependency.split("@", 1)
    else:
        name, requirement = args.dependency, "*"
    name = validate_name(name)
    requirement = requirement or "*"
    lines = manifest_path.read_text(encoding="utf-8").splitlines()
    start = None
    end = len(lines)
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped == "[dependencies]":
            start = index + 1
            continue
        if start is not None and stripped.startswith("["):
            end = index
            break
    if start is None:
        if lines and lines[-1].strip():
            lines.append("")
        lines += ["[dependencies]", _dependency_line(name, requirement)]
    else:
        pattern = re.compile(rf"^\s*{re.escape(name)}\s*=")
        replaced = False
        for index in range(start, end):
            if pattern.match(lines[index]):
                lines[index] = _dependency_line(name, requirement)
                replaced = True
                break
        if not replaced:
            lines.insert(end, _dependency_line(name, requirement))
    manifest_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"added dependency {name} {requirement}")
    return 0


def cmd_fetch(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    offline = getattr(args, "offline", False)
    if offline:
        manifest, packages = resolve_offline(root)
    else:
        manifest, packages = resolve(root, update=args.update, registry_override=args.registry)
    for package in packages:
        materialize(package, root, offline=offline)
    if not offline:
        write_lock(root, manifest, packages)
    print(f"resolved and fetched {len(packages)} package(s)")
    return 0


def cmd_tree(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    records = lock_records(root)
    if not (root / "Basalt.lock").exists():
        raise PackageError("Basalt.lock is missing; run fetch first")
    manifest = load_manifest(root)
    print(f"{manifest['name']} {manifest['version']}")
    for name in sorted(records):
        record = records[name]
        deps = record.get("dependencies", [])
        suffix = " -> " + ", ".join(str(item) for item in deps) if deps else ""
        print(f"  {name} {record['version']} [{record.get('source', 'unknown')}]{suffix}")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    _manifest, packages = resolve_offline(root)
    for package in packages:
        vendor = root / ".basalt" / "vendor" / package.name / str(package.version)
        if not vendor.is_dir():
            raise PackageError(f"vendor package is missing for {package.identity}")
        package_manifest = load_manifest(vendor)
        if package_manifest["name"] != package.name or package_manifest["version"] != package.version:
            raise PackageError(f"vendor manifest mismatch for {package.identity}")
        if package.checksum:
            cache = cache_path(package.checksum)
            if not cache.is_file():
                raise PackageError(f"cached archive is missing for {package.identity}")
            if sha256_file(cache) != package.checksum.removeprefix("sha256:"):
                raise PackageError(f"cached archive checksum mismatch for {package.name}")
        elif package.path is None:
            raise PackageError(f"path package has no source path: {package.identity}")
    print(f"verified manifest and lockfile for {len(packages)} package(s)")
    return 0


def cmd_build(args: argparse.Namespace) -> int:
    root = args.root.resolve()
    cmd_fetch(argparse.Namespace(root=root, update=False, registry=args.registry, offline=args.offline))
    manifest = load_manifest(root)
    output = (root / args.output).resolve() if args.output else root / ".basalt" / "bin" / manifest["name"]
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        args.compiler,
        "--compile",
        str(manifest["entry_path"]),
        "--cc",
        args.cc,
        "-o",
        str(output),
        "--",
    ]
    command.extend(args.compiler_arg or [])
    if args.dry_run:
        print(" ".join(json.dumps(part) for part in command))
        return 0
    print("running compiler:", " ".join(json.dumps(part) for part in command))
    completed = subprocess.run(command, cwd=root, check=False)
    return completed.returncode


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="basalt-pkg", description="Basalt package manager")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="package root, default: current directory")
    parser.add_argument("--registry", default=os.environ.get("BASALT_REGISTRY", ""), help="registry base URL or local directory")
    sub = parser.add_subparsers(dest="command", required=True)

    init = sub.add_parser("init", help="create Basalt.toml")
    init.add_argument("name", nargs="?")
    init.add_argument("--force", action="store_true")
    init.set_defaults(function=cmd_init)

    add = sub.add_parser("add", help="add a manifest dependency")
    add.add_argument("dependency", help="NAME or NAME@REQUIREMENT")
    add.set_defaults(function=cmd_add)

    fetch = sub.add_parser("fetch", help="resolve, verify and materialize dependencies")
    fetch.add_argument("--update", action="store_true")
    fetch.add_argument("--offline", action="store_true", help="use only Basalt.lock and cached archives")
    fetch.set_defaults(function=cmd_fetch)

    update = sub.add_parser("update", help="re-resolve dependencies")
    update.set_defaults(function=lambda args: cmd_fetch(argparse.Namespace(root=args.root, update=True, registry=args.registry, offline=False)))

    tree = sub.add_parser("tree", help="print the locked dependency graph")
    tree.set_defaults(function=cmd_tree)

    verify = sub.add_parser("verify", help="verify manifest, lockfile and cached archives")
    verify.set_defaults(function=cmd_verify)

    build = sub.add_parser("build", help="fetch dependencies and invoke basaltc")
    build.add_argument("--compiler", default=os.environ.get("BASALT_COMPILER", "basaltc"), help="Bootstrap compiler executable")
    build.add_argument("--cc", default=os.environ.get("CC", "cc"), help="C compiler used by Bootstrap compiler")
    build.add_argument("--output")
    build.add_argument("--compiler-arg", action="append", default=[])
    build.add_argument("--offline", action="store_true", help="use only Basalt.lock and cached archives")
    build.add_argument("--dry-run", action="store_true")
    build.set_defaults(function=cmd_build)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.function(args))
    except PackageError as exc:
        print(f"basalt-pkg: error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("basalt-pkg: interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
