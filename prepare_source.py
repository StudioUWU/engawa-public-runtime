#!/usr/bin/env python3
# Copyright (C) 2026 Dill - StudioUWU - EngawaRuntime.
# SPDX-License-Identifier: BSD-2-Clause
"""Verify and reconstruct the public engine source in a new directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

def safe(name):
    if (not isinstance(name, str) or not name or "\\" in name or ":" in name
            or "\0" in name or name.startswith("/")
            or any(p in ("", ".", "..") or p.lower() == ".git" for p in name.split("/"))):
        raise RuntimeError("Unsafe source path: " + repr(name))
    return name

def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            result.update(block)
    return result.hexdigest()

def parents_plain(root, path):
    for parent in (path, *path.parents):
        if parent == root:
            break
        if parent.is_symlink():
            raise RuntimeError("Source path traverses a symbolic link")

def remove_readonly(function, path, error):
    # Git pack files can be read-only on Windows. Clean up only our staging
    # directory on failure, including an interrupted fetch or rejected pin.
    if not isinstance(error[1], PermissionError):
        raise error[1]
    os.chmod(path, 0o700)
    function(path)

def prepare(bundle, destination):
    manifest = json.loads((bundle / "SOURCE_MANIFEST.json").read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 2:
        raise RuntimeError("Unsupported source candidate manifest")
    identity = json.dumps(manifest["source_identity"], sort_keys=True,
                          separators=(",", ":")).encode()
    if hashlib.sha256(identity).hexdigest() != manifest.get("source_identity_sha256"):
        raise RuntimeError("Public source identity digest mismatch")
    seen = set()
    for entry in manifest["files"]:
        name = safe(entry["path"])
        if name in seen:
            raise RuntimeError("Duplicate source manifest path")
        seen.add(name)
        path = bundle / name
        parents_plain(bundle, path)
        if not path.is_file() or digest(path) != entry["sha256"]:
            raise RuntimeError("Source candidate hash mismatch: " + name)
    for required in ("Patches/WebKit.patch", "prepare_source.py"):
        if required not in seen:
            raise RuntimeError("Missing required source input: " + required)
    overlay_entries = manifest["source_identity"]["overlay"]
    source_identity = manifest["source_identity"]
    commit, tree = source_identity["upstream_commit"], source_identity["upstream_tree"]
    url = source_identity["upstream_url"]
    if url != "https://github.com/WebKit/WebKit.git":
        raise RuntimeError("Expected the official WebKit Git remote")
    if not all(isinstance(oid, str) and re.fullmatch(r"[0-9a-f]{40}", oid)
               for oid in (commit, tree)):
        raise RuntimeError("Expected exact WebKit commit and tree object IDs")
    if digest(bundle / "Patches/WebKit.patch") != source_identity["patch_sha256"]:
        raise RuntimeError("Public patch identity mismatch")
    for entry in overlay_entries:
        name = safe(entry["path"])
        if "Overlay/" + name not in seen or digest(bundle / "Overlay" / name) != entry["sha256"]:
            raise RuntimeError("Public overlay identity mismatch: " + name)
    for ancestor in (destination, *destination.parents):
        if ancestor.is_symlink():
            raise RuntimeError("Source destination traverses a symbolic link")
    if destination.exists():
        raise RuntimeError("Source destination already exists")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".public-source-", dir=destination.parent))
    try:
        environment = dict(os.environ)
        for key in tuple(environment):
            if key.startswith("GIT_"):
                del environment[key]
        subprocess.run(["git", "init", "--quiet", "--template=", "--object-format=sha1", str(stage)],
                       check=True, env=environment)
        # Preserve pinned blob bytes and real symlinks regardless of host
        # checkout filters. Never import private repository objects or history.
        attributes = " -text -eol -ident -filter -working-tree-encoding\n"
        (stage / ".git/info").mkdir(exist_ok=True)
        (stage / ".git/info/attributes").write_bytes(("*" + attributes + "**" + attributes).encode())
        command = ["git", "-c", "core.autocrlf=false", "-c", "core.symlinks=true",
                   "-c", "core.longpaths=true", "-c", "core.hooksPath=",
                   "-c", "core.attributesFile=", "-C", str(stage)]
        def git(*arguments):
            return subprocess.check_output(command + list(arguments), env=environment,
                                           text=True).strip()
        git("remote", "add", "origin", url)
        git("fetch", "--depth=1", "--no-tags", "--no-recurse-submodules", "origin", commit)
        if git("rev-parse", "FETCH_HEAD") != commit or git("rev-parse", commit + "^{tree}") != tree:
            raise RuntimeError("Fetched WebKit commit/tree mismatch")
        git("checkout", "--detach", "--force", commit)
        git("config", "core.autocrlf", "false")
        git("config", "core.symlinks", "true")
        git("config", "core.longpaths", "true")
        patch = str(bundle / "Patches" / "WebKit.patch")
        git("apply", "--check", patch)
        git("apply", "--whitespace=nowarn", patch)
        for entry in overlay_entries:
            target = stage / entry["path"]
            parents_plain(stage, target)
            if target.exists() or target.is_symlink():
                raise RuntimeError("Public overlay collides with upstream: " + entry["path"])
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(bundle / "Overlay" / entry["path"], target)
            target.chmod(0o755 if entry["mode"] == "100755" else 0o644)
        if (stage / "Source/EngawaRuntime").exists():
            raise RuntimeError("Private runtime source appeared in public reconstruction")
        if destination.exists():
            raise RuntimeError("Source destination appeared during reconstruction")
        stage.rename(destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage, onerror=remove_readonly)
    return destination

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    print(prepare(Path(__file__).resolve().parent, args.destination.absolute()))
