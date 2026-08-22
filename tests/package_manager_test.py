#!/usr/bin/env python3
"""Black-box tests for scripts/basalt_pkg.py.

The test registry is local and deterministic. No network, package script, or
compiler Host is executed.
"""

from __future__ import annotations

import hashlib
import io
import json
import os
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "basalt_pkg.py"
TEST_TMP = ROOT / ".tmp" / "package-manager-tests"


def temporary_workspace(prefix: str) -> tempfile.TemporaryDirectory[str]:
    TEST_TMP.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix=prefix, dir=TEST_TMP)


def run(*args: str, cwd: Path, env: dict[str, str] | None = None, check: bool = True) -> subprocess.CompletedProcess[str]:
    merged = os.environ.copy()
    if env:
        merged.update(env)
    return subprocess.run(
        [sys.executable, str(TOOL), *args],
        cwd=cwd,
        env=merged,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=check,
    )


def archive_package(registry: Path, name: str, version: str, dependencies: dict[str, str] | None = None) -> str:
    package_root = registry / "staging" / f"{name}-{version}"
    (package_root / "src").mkdir(parents=True, exist_ok=True)
    manifest = (
        "[package]\n"
        f'name = "{name}"\n'
        f'version = "{version}"\n'
        'entry = "src/main.basalt"\n\n'
        "[dependencies]\n"
    )
    if dependencies:
        for dep_name, requirement in sorted(dependencies.items()):
            manifest += f'{dep_name} = "{requirement}"\n'
    (package_root / "Basalt.toml").write_text(manifest, encoding="utf-8")
    (package_root / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
    archive_dir = registry / "archives"
    archive_dir.mkdir(parents=True, exist_ok=True)
    archive = archive_dir / f"{name}-{version}.tar.gz"
    with tarfile.open(archive, "w:gz") as output:
        output.add(package_root, arcname=f"{name}-{version}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    index_dir = registry / "index"
    index_dir.mkdir(parents=True, exist_ok=True)
    index_path = index_dir / f"{name}.json"
    existing = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    existing.append(
        {
            "name": name,
            "version": version,
            "archive": f"archives/{archive.name}",
            "checksum": f"sha256:{digest}",
            "dependencies": dependencies or {},
        }
    )
    index_path.write_text(json.dumps(existing, indent=2) + "\n", encoding="utf-8")
    return digest


def write_index_record(registry: Path, record: dict[str, object]) -> None:
    index_dir = registry / "index"
    index_dir.mkdir(parents=True, exist_ok=True)
    name = str(record["name"])
    (index_dir / f"{name}.json").write_text(json.dumps([record], indent=2) + "\n", encoding="utf-8")


def custom_archive(
    registry: Path,
    archive_name: str,
    top_level: str,
    manifest_name: str,
    version: str,
    dependencies: dict[str, str] | None = None,
    symlink: bool = False,
    traversal: str | None = None,
) -> Path:
    archive_dir = registry / "archives"
    archive_dir.mkdir(parents=True, exist_ok=True)
    archive = archive_dir / archive_name
    manifest = (
        "[package]\n"
        f'name = "{manifest_name}"\n'
        f'version = "{version}"\n'
        'entry = "src/main.basalt"\n\n'
        "[dependencies]\n"
    )
    for dep_name, requirement in sorted((dependencies or {}).items()):
        manifest += f'{dep_name} = "{requirement}"\n'
    with tarfile.open(archive, "w:gz") as output:
        directory = tarfile.TarInfo(top_level)
        directory.type = tarfile.DIRTYPE
        output.addfile(directory)
        manifest_info = tarfile.TarInfo(f"{top_level}/Basalt.toml")
        manifest_bytes = manifest.encode("utf-8")
        manifest_info.size = len(manifest_bytes)
        output.addfile(manifest_info, io.BytesIO(manifest_bytes))
        if symlink:
            link = tarfile.TarInfo(f"{top_level}/src")
            link.type = tarfile.SYMTYPE
            link.linkname = "/etc"
            output.addfile(link)
        else:
            source_directory = tarfile.TarInfo(f"{top_level}/src")
            source_directory.type = tarfile.DIRTYPE
            output.addfile(source_directory)
            source_info = tarfile.TarInfo(f"{top_level}/src/main.basalt")
            source_bytes = b"func main(): int { return 0; }\n"
            source_info.size = len(source_bytes)
            output.addfile(source_info, io.BytesIO(source_bytes))
        if traversal is not None:
            payload = b"escape"
            traversal_info = tarfile.TarInfo(traversal)
            traversal_info.size = len(payload)
            output.addfile(traversal_info, io.BytesIO(payload))
    return archive


def make_project(root: Path) -> None:
    (root / "src").mkdir(parents=True)
    (root / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
    (root / "Basalt.toml").write_text(
        "[package]\n"
        'name = "demo-app"\n'
        'version = "0.1.0"\n'
        'entry = "src/main.basalt"\n\n'
        "[dependencies]\n"
        'widget = { version = "^1.2.0" }\n',
        encoding="utf-8",
    )


def test_registry_resolution_and_lockfile() -> None:
    with temporary_workspace(prefix="basalt-pkg-test-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        make_project(project)
        old_digest = archive_package(registry, "widget", "1.1.0")
        selected_digest = archive_package(registry, "widget", "1.2.0")
        archive_package(registry, "widget", "1.3.0")
        fetched = run("--registry", str(registry), "fetch", cwd=project)
        assert "resolved and fetched 1 package(s)" in fetched.stdout
        lock = (project / "Basalt.lock").read_text(encoding="utf-8")
        assert 'name = "widget"' in lock
        assert 'version = "1.3.0"' in lock
        assert "sha256:" in lock
        assert old_digest not in lock
        assert selected_digest not in lock
        assert (project / ".basalt/vendor/widget/1.3.0/Basalt.toml").is_file()
        tree = run("tree", cwd=project)
        assert "widget 1.3.0" in tree.stdout
        verified = run("verify", cwd=project)
        assert "verified manifest and lockfile" in verified.stdout

        cache_files = list((Path(os.environ.get("BASALT_HOME", str(Path.home() / ".basalt"))) / "cache").rglob("*.tar.gz"))
        assert cache_files, "fetch must populate the content-addressed cache"


def test_lockfile_pins_until_update() -> None:
    with temporary_workspace(prefix="basalt-pkg-lock-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        make_project(project)
        archive_package(registry, "widget", "1.2.0")
        archive_package(registry, "widget", "1.3.0")
        run("--registry", str(registry), "fetch", cwd=project)
        # Add a newer compatible record after lock creation. A normal fetch is pinned.
        archive_package(registry, "widget", "1.4.0")
        run("--registry", str(registry), "fetch", cwd=project)
        lock = (project / "Basalt.lock").read_text(encoding="utf-8")
        assert 'version = "1.3.0"' in lock
        run("--registry", str(registry), "update", cwd=project)
        updated = (project / "Basalt.lock").read_text(encoding="utf-8")
        assert 'version = "1.4.0"' in updated


def test_semver_selection_and_prerelease() -> None:
    cases = [
        ("1.2.0", "1.2.0"),
        ("^1.2.0", "1.4.0"),
        ("~1.2.0", "1.2.5"),
        (">=1.2.0, <1.3.0", "1.2.5"),
        ("1.2", "1.2.5"),
        ("*", "2.0.0"),
        ("=1.3.0-alpha", "1.3.0-alpha"),
        ("^1.3.0", "1.4.0"),
        ("~0.2.3", "0.2.9"),
    ]
    with temporary_workspace("basalt-pkg-semver-") as temp:
        root = Path(temp)
        registry = root / "registry"
        for version in ("0.2.3", "0.2.9", "0.3.0", "1.2.0", "1.2.5", "1.3.0-alpha", "1.3.0", "1.4.0", "2.0.0"):
            archive_package(registry, "widget", version)
        for index, (requirement, expected) in enumerate(cases):
            project = root / f"project-{index}"
            make_project(project)
            manifest = (project / "Basalt.toml").read_text(encoding="utf-8")
            (project / "Basalt.toml").write_text(manifest.replace('widget = { version = "^1.2.0" }', f'widget = "{requirement}"'), encoding="utf-8")
            selected = run("--registry", str(registry), "fetch", cwd=project, check=False)
            assert selected.returncode == 0, (requirement, selected.stdout, selected.stderr)
            lock = (project / "Basalt.lock").read_text(encoding="utf-8")
            assert f'version = "{expected}"' in lock, (requirement, expected, lock)

        invalid = root / "invalid"
        (invalid / "src").mkdir(parents=True)
        (invalid / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (invalid / "Basalt.toml").write_text(
            "[package]\nname = \"invalid-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\ninvalid = \"1.0.0-01\"\n",
            encoding="utf-8",
        )
        archive_package(registry, "invalid", "1.0.0")
        failed = run("--registry", str(registry), "fetch", cwd=invalid, check=False)
        assert failed.returncode == 2
        assert "invalid SemVer" in failed.stderr


def test_constraint_intersection_and_conflict() -> None:
    with temporary_workspace(prefix="basalt-pkg-resolve-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        (project / "src").mkdir(parents=True)
        (project / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (project / "Basalt.toml").write_text(
            "[package]\nname = \"intersection-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\nalpha = \"1.0.0\"\nbeta = \"1.0.0\"\n",
            encoding="utf-8",
        )
        archive_package(registry, "common", "1.0.0")
        archive_package(registry, "common", "1.1.0")
        archive_package(registry, "common", "1.2.0")
        archive_package(registry, "alpha", "1.0.0", {"common": ">=1.0.0, <2.0.0"})
        archive_package(registry, "beta", "1.0.0", {"common": ">=1.0.0, <1.2.0"})
        run("--registry", str(registry), "fetch", cwd=project)
        lock = (project / "Basalt.lock").read_text(encoding="utf-8")
        assert 'name = "common"' in lock
        assert 'version = "1.1.0"' in lock

        archive_package(registry, "beta", "1.0.1", {"common": ">=2.0.0"})
        backtrack = root / "backtrack"
        (backtrack / "src").mkdir(parents=True)
        (backtrack / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (backtrack / "Basalt.toml").write_text(
            "[package]\nname = \"backtrack-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\nchoice = \"*\"\ncommon = \"^1.0.0\"\n",
            encoding="utf-8",
        )
        archive_package(registry, "choice", "1.0.0", {"common": "^1.0.0"})
        archive_package(registry, "choice", "2.0.0", {"common": "^2.0.0"})
        archive_package(registry, "common", "2.0.0")
        run("--registry", str(registry), "fetch", cwd=backtrack)
        backtrack_lock = (backtrack / "Basalt.lock").read_text(encoding="utf-8")
        assert 'name = "choice"' in backtrack_lock
        assert 'version = "1.0.0"' in backtrack_lock
        assert 'name = "common"' in backtrack_lock
        assert 'version = "1.2.0"' in backtrack_lock

        (project / "Basalt.toml").write_text(
            "[package]\nname = \"conflict-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\nalpha = \"1.0.0\"\nbeta = \"1.0.1\"\n",
            encoding="utf-8",
        )
        failed = run("--registry", str(registry), "update", cwd=project, check=False)
        assert failed.returncode == 2
        assert "no registry version of common satisfies all requirements" in failed.stderr


def test_offline_and_lock_validation() -> None:
    with temporary_workspace(prefix="basalt-pkg-offline-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        make_project(project)
        archive_package(registry, "widget", "1.2.0")
        run("--registry", str(registry), "fetch", cwd=project)
        offline = run("--registry", str(root / "does-not-exist"), "fetch", "--offline", cwd=project)
        assert "resolved and fetched 1 package(s)" in offline.stdout
        lock = (project / "Basalt.lock").read_text(encoding="utf-8")
        (project / "Basalt.lock").write_text(
            lock + "\n[[package]]\nname = \"widget\"\nversion = \"1.2.0\"\n"
            "source = \"registry+unused\"\nchecksum = \"sha256:" + "0" * 64 + "\"\n"
            "archive = \"archives/widget-1.2.0.tar.gz\"\ndependencies = []\n",
            encoding="utf-8",
        )
        failed = run("verify", cwd=project, check=False)
        assert failed.returncode == 2
        assert "duplicate package record" in failed.stderr


def test_path_dependency_and_dry_run() -> None:
    with temporary_workspace(prefix="basalt-pkg-path-") as temp:
        root = Path(temp)
        shared = root / "shared-lib"
        leaf = root / "local-dep"
        project = root / "project"
        (leaf / "src").mkdir(parents=True)
        (leaf / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (leaf / "Basalt.toml").write_text(
            "[package]\nname = \"local-dep\"\nversion = \"0.2.0\"\nentry = \"src/main.basalt\"\n",
            encoding="utf-8",
        )
        (shared / "src").mkdir(parents=True)
        (shared / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (shared / "Basalt.toml").write_text(
            "[package]\nname = \"shared-lib\"\nversion = \"0.5.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\nlocal-dep = { path = \"../local-dep\", version = \"^0.2.0\" }\n",
            encoding="utf-8",
        )
        (project / "src").mkdir(parents=True)
        (project / "src/main.basalt").write_text("func main(): int { return 0; }\n", encoding="utf-8")
        (project / "Basalt.toml").write_text(
            "[package]\nname = \"path-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n\n"
            "[dependencies]\nshared-lib = { path = \"../shared-lib\", version = \"^0.5.0\" }\n",
            encoding="utf-8",
        )
        run("fetch", cwd=project)
        run("verify", cwd=project)
        assert (project / ".basalt/vendor/shared-lib/0.5.0/Basalt.toml").is_file()
        assert (project / ".basalt/vendor/local-dep/0.2.0/Basalt.toml").is_file()
        dry = run(
            "build",
            "--compiler",
            "basaltc with space",
            "--cc",
            "cc with space",
            "--compiler-arg=-std=c11",
            "--dry-run",
            cwd=project,
        )
        assert '"basaltc with space"' in dry.stdout
        assert '"cc with space"' in dry.stdout
        assert '"-std=c11"' in dry.stdout


def test_cli_init_add_and_build_argv() -> None:
    with temporary_workspace(prefix="basalt-pkg-cli-") as temp:
        root = Path(temp)
        project = root / "project"
        project.mkdir()
        initialized = run("init", "cli-app", cwd=project)
        assert "initialized package cli-app" in initialized.stdout
        added = run("add", "text@^1.2.0", cwd=project)
        assert "added dependency text ^1.2.0" in added.stdout
        manifest = (project / "Basalt.toml").read_text(encoding="utf-8")
        assert 'text = "^1.2.0"' in manifest

        recorder = project / "compiler stub.py"
        argv_file = project / "compiler-argv.json"
        recorder.write_text(
            "#!/usr/bin/env python3\n"
            "import json\n"
            "import pathlib\n"
            "import sys\n"
            f"pathlib.Path({str(argv_file)!r}).write_text(json.dumps(sys.argv[1:]), encoding='utf-8')\n",
            encoding="utf-8",
        )
        recorder.chmod(0o755)
        # Replace the generated dependency table with no dependencies so the stub test is offline.
        (project / "Basalt.toml").write_text(
            "[package]\nname = \"cli-app\"\nversion = \"0.1.0\"\nentry = \"src/main.basalt\"\n",
            encoding="utf-8",
        )
        run("fetch", cwd=project)
        build = run(
            "build",
            "--offline",
            "--compiler",
            str(recorder),
            "--cc",
            "cc with space",
            "--output",
            ".tmp/output with space.bin",
            "--compiler-arg=-DVALUE=hello world",
            "--compiler-arg=-std=c11",
            cwd=project,
            check=False,
        )
        assert build.returncode == 0, (build.stdout, build.stderr)
        assert json.loads(argv_file.read_text(encoding="utf-8")) == [
            "--compile",
            str(project / "src/main.basalt"),
            "--cc",
            "cc with space",
            "-o",
            str(project / ".tmp/output with space.bin"),
            "--",
            "-DVALUE=hello world",
            "-std=c11",
        ]


def test_registry_schema_rejections() -> None:
    with temporary_workspace(prefix="basalt-pkg-registry-schema-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        make_project(project)
        archive_package(registry, "widget", "1.2.0")
        index_path = registry / "index/widget.json"
        records = json.loads(index_path.read_text(encoding="utf-8"))
        records.append(dict(records[0]))
        index_path.write_text(json.dumps(records) + "\n", encoding="utf-8")
        duplicate = run("--registry", str(registry), "fetch", cwd=project, check=False)
        assert duplicate.returncode == 2
        assert "duplicate version" in duplicate.stderr

        invalid_registries = {
            "http://127.0.0.1:1": "registry must be a local directory",
            "file://remote.invalid/registry": "file registry must not contain a remote host",
            "https://user:pass@example.invalid/registry": "registry must be a local directory",
        }
        for registry_url, expected in invalid_registries.items():
            failed = run("--registry", registry_url, "fetch", cwd=project, check=False)
            assert failed.returncode == 2, (registry_url, failed.stdout, failed.stderr)
            assert expected in failed.stderr, (registry_url, failed.stderr)


def test_archive_validation_matrix() -> None:
    cases = [
        ("absolute-path.tar.gz", "widget-1.2.0", "widget", "1.2.0", {}, False, None, "/tmp/not-allowed.tar.gz", "invalid archive path"),
        ("traversal.tar.gz", "widget-1.2.0", "widget", "1.2.0", {}, False, "widget-1.2.0/../../escape", "archives/traversal.tar.gz", "archive path traversal"),
        ("symlink.tar.gz", "widget-1.2.0", "widget", "1.2.0", {}, True, None, "archives/symlink.tar.gz", "unsupported archive member type"),
        ("wrong-top.tar.gz", "wrong-1.2.0", "widget", "1.2.0", {}, False, None, "archives/wrong-top.tar.gz", "must be under widget-1.2.0/"),
        ("wrong-manifest.tar.gz", "widget-1.2.0", "other", "1.2.0", {}, False, None, "archives/wrong-manifest.tar.gz", "archive manifest identity mismatch"),
        ("wrong-version.tar.gz", "widget-1.2.0", "widget", "1.2.1", {}, False, None, "archives/wrong-version.tar.gz", "archive manifest identity mismatch"),
        ("wrong-dependencies.tar.gz", "widget-1.2.0", "widget", "1.2.0", {"extra": "^1.0.0"}, False, None, "archives/wrong-dependencies.tar.gz", "archive manifest dependencies mismatch"),
    ]
    for index, (archive_name, top_level, manifest_name, version, archive_dependencies, symlink, traversal, archive_ref, expected) in enumerate(cases):
        with temporary_workspace(f"basalt-pkg-archive-{index}-") as temp:
            root = Path(temp)
            registry = root / "registry"
            project = root / "project"
            make_project(project)
            archive = custom_archive(
                registry,
                archive_name,
                top_level,
                manifest_name,
                version,
                archive_dependencies,
                symlink=symlink,
                traversal=traversal,
            )
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            record = {
                "name": "widget",
                "version": "1.2.0",
                "archive": archive_ref,
                "checksum": f"sha256:{digest}",
                "dependencies": {},
            }
            write_index_record(registry, record)
            failed = run("--registry", str(registry), "fetch", cwd=project, check=False)
            assert failed.returncode == 2, (expected, failed.returncode, failed.stdout, failed.stderr)
            assert expected in failed.stderr


def test_checksum_and_archive_traversal_fail_closed() -> None:
    with temporary_workspace(prefix="basalt-pkg-security-") as temp:
        root = Path(temp)
        registry = root / "registry"
        project = root / "project"
        make_project(project)
        archive_package(registry, "widget", "1.2.0")
        run("--registry", str(registry), "fetch", cwd=project)
        lock_text = (project / "Basalt.lock").read_text(encoding="utf-8")
        checksum = next(line.split('"', 2)[1] for line in lock_text.splitlines() if line.startswith("checksum = "))
        digest = checksum.removeprefix("sha256:")
        cache_file = Path(os.environ["BASALT_HOME"]) / "cache" / "sha256" / digest[:2] / f"{digest}.tar.gz"
        assert cache_file.is_file()
        cache_file.write_bytes(b"tampered")
        failed = run("verify", cwd=project, check=False)
        assert failed.returncode == 2, (failed.returncode, failed.stdout, failed.stderr)
        assert "checksum mismatch" in failed.stderr

        bad = registry / "bad.tar.gz"
        with tarfile.open(bad, "w:gz") as output:
            payload = root / "payload.txt"
            payload.write_text("do not extract", encoding="utf-8")
            info = tarfile.TarInfo("widget-1.2.0/../../escape.txt")
            info.size = payload.stat().st_size
            with payload.open("rb") as source:
                output.addfile(info, source)
        bad_digest = hashlib.sha256(bad.read_bytes()).hexdigest()
        (registry / "index/widget.json").write_text(
            json.dumps([{
                "name": "widget",
                "version": "1.2.0",
                "archive": "bad.tar.gz",
                "checksum": f"sha256:{bad_digest}",
                "dependencies": {},
            }]) + "\n",
            encoding="utf-8",
        )
        traversal = run("--registry", str(registry), "update", cwd=project, check=False)
        assert traversal.returncode == 2
        assert "archive path traversal" in traversal.stderr


def main() -> None:
    previous_home = os.environ.get("BASALT_HOME")
    try:
        with temporary_workspace("basalt-pkg-home-") as home:
            os.environ["BASALT_HOME"] = home
            test_registry_resolution_and_lockfile()
            test_lockfile_pins_until_update()
            test_semver_selection_and_prerelease()
            test_constraint_intersection_and_conflict()
            test_offline_and_lock_validation()
            test_path_dependency_and_dry_run()
            test_cli_init_add_and_build_argv()
            test_registry_schema_rejections()
            test_archive_validation_matrix()
            test_checksum_and_archive_traversal_fail_closed()
    finally:
        if previous_home is None:
            os.environ.pop("BASALT_HOME", None)
        else:
            os.environ["BASALT_HOME"] = previous_home
    print("package manager tests: PASS")


if __name__ == "__main__":
    main()
