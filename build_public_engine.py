#!/usr/bin/env python3
# Copyright (C) 2026 EngawaRuntime contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Build and install the public engine independently of the private SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

PROTECTED = {
    "PORT": "Engawa", "ER_BUILD_PUBLIC_ENGINE_ONLY": "ON",
    "ER_ENABLE_WEBKIT_ENGINE": "OFF", "ER_ENABLE_INPROCESS_ENGINE": "ON",
    "ER_ENABLE_INPROCESS_SHARED_LIBRARIES": "ON",
    "ER_ENABLE_INPROCESS_MONOLITHIC_LINK": "OFF",
    "ENABLE_ENGAWARUNTIME_TESTS": "OFF",
    "ENABLE_API_TESTS": "OFF", "ENABLE_LAYOUT_TESTS": "OFF",
    "ENABLE_MINIBROWSER": "OFF", "ENABLE_WEBINSPECTORUI": "OFF",
    "ENABLE_WEBKIT_LEGACY": "OFF",
}
if sys.platform == "darwin":
    PROTECTED["USE_APPLE_ICU"] = "OFF"

def path_without_links(value):
    value = value.absolute()
    if any(p.is_symlink() for p in (value, *value.parents)):
        raise RuntimeError("Build paths may not traverse symbolic links")
    return value.resolve()

def run(args):
    source = path_without_links(args.source)
    build = path_without_links(args.build)
    install = path_without_links(args.install)
    for left, right in ((source, build), (source, install), (build, install)):
        if left == right or left in right.parents or right in left.parents:
            raise RuntimeError("Source, build and install must be separate, nonnested directories")
    if not (source / "CMakeLists.txt").is_file():
        raise RuntimeError("Prepare the public WebKit source before building")
    if (source / "Source/EngawaRuntime").exists():
        raise RuntimeError("This runner accepts only the public source projection")
    if args.jobs < 1:
        raise RuntimeError("--jobs must be positive")
    user_arguments = []
    for argument in args.cmake_arg:
        match = re.fullmatch(r"-D([A-Za-z0-9_]+)(?::[A-Za-z]+)?=(.*)", argument)
        if not match:
            raise RuntimeError("--cmake-arg accepts only one -DNAME[:TYPE]=VALUE per argument")
        key, value = match.groups()
        if key in PROTECTED or key in ("CMAKE_INSTALL_PREFIX", "CMAKE_BUILD_TYPE",
                                      "ER_ENABLE_INPROCESS_UTILS_DLL"):
            raise RuntimeError("Public build policy cannot be overridden: " + key)
        user_arguments.append(argument)
    definitions = ["-D" + key + "=" + value for key, value in PROTECTED.items()]
    definitions.extend(["-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(install)])
    if os.name == "nt":
        definitions.append("-DER_ENABLE_INPROCESS_UTILS_DLL=ON")
    else:
        definitions.append("-DER_ENABLE_INPROCESS_UTILS_DLL=OFF")
    commands = [
        [args.cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
         *user_arguments, *definitions],
        [args.cmake, "--build", str(build), "--target", "EngawaPublicEngine",
         "--parallel", str(args.jobs)],
        [args.cmake, "--install", str(build), "--component", "EngawaRuntimeBootstrap"],
    ]
    build.mkdir(parents=True, exist_ok=True)
    receipt = {"schema_version": 1, "candidate_only": True, "steps": [],
               "scope": "public source compilation and installation only",
               "recipient_replacement_verified": False,
               "release_binary_correspondence_verified": False,
               "legal_review_complete": False}
    receipt_path = build / "public-build-result.json"
    try:
        for index, command in enumerate(commands, start=1):
            log = build / ("public-build-" + str(index) + ".log")
            with log.open("w", encoding="utf-8") as stream:
                process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT, text=True,
                                           errors="replace", bufsize=1)
                try:
                    for line in process.stdout:
                        print(line, end="", flush=True)
                        stream.write(line)
                        stream.flush()
                    code = process.wait()
                except BaseException:
                    process.kill()
                    process.wait()
                    raise
                finally:
                    process.stdout.close()
            receipt["steps"].append({"command": command, "exit_code": code,
                                     "log": log.name})
            if code:
                raise RuntimeError("Public engine command failed; see " + str(log))
        prefix, suffix = ("", ".dll") if os.name == "nt" else (
            "lib", ".dylib" if sys.platform == "darwin" else ".so")
        expected = ["bin/" + prefix + name + suffix
                    for name in ("WebCore", "JavaScriptCore", "EngawaRuntimeUtils")]
        expected.append("include/engawa_webcore.h")
        artifacts = []
        for relative in expected:
            path = install / relative
            if not path.is_file():
                raise RuntimeError("Missing installed public artifact: " + relative)
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1048576), b""):
                    digest.update(block)
            artifacts.append({"path": relative, "sha256": digest.hexdigest()})
        if (install / ("bin/" + prefix + "EngawaRuntime" + suffix)).exists():
            raise RuntimeError("Private runtime appeared in the public install")
        receipt["artifacts"] = artifacts
        receipt["public_build_install_passed"] = True
    finally:
        receipt_path.write_bytes((json.dumps(receipt, indent=2) + "\n").encode())
    print("Public engine installed; replacement and release correspondence remain unverified.")
    return receipt

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--install", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cmake-arg", action="append", default=[],
                        help="Use --cmake-arg=-DNAME=VALUE for each tool/dependency setting")
    try:
        run(parser.parse_args())
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, "Public engine build refused: " + str(error) + "\n")
