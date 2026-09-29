"""Record the activated Windows CI toolchain without dumping secrets or the full environment.

Run after vcvars and Conan profile detection, with the selected LLVM first on PATH.
The report identifies this run; it is not a dependency lockfile or a cache key.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    commands = {
        "compiler": ["clang-cl", "--version"],
        "asan_resource_dir": ["clang-cl", "--print-resource-dir"],
        "linker": ["lld-link", "--version"],
        "cmake": ["cmake", "--version"],
        "ninja": ["ninja", "--version"],
        "conan": ["conan", "--version"],
        "python": [sys.executable, "--version"],
        "python_packages": [sys.executable, "-m", "pip", "freeze", "--all"],
        "profiles": ["conan", "profile", "show", "-pr:h", "profiles/conan/windows-clang-cl-ci",
                     "-pr:b", "default", "-s:h", "build_type=Release", "-s:h", "compiler.cppstd=23"],
        "qt": [str(Path(os.environ["DSS_QT_ROOT"]) / "bin/qmake.exe"), "-query"],
    }
    report = {"environment": {key: os.environ.get(key) for key in (
        "GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "ImageOS", "ImageVersion",
        "VSINSTALLDIR", "VisualStudioVersion", "VCToolsVersion", "VCToolsInstallDir",
        "WindowsSdkDir", "WindowsSDKVersion", "CONAN_HOME", "DSS_QT_ROOT", "DSS_LLVM_BIN")},
        "inputs": {}, "tools": {}}
    inputs = [Path("conanfile.py"), Path("profiles/conan/windows-clang-cl-ci"),
              Path(os.environ["CONAN_HOME"]) / "settings_user.yml",
              Path(os.environ["CONAN_HOME"]) / "profiles/default"]
    for path in inputs:
        content = path.read_bytes()
        report["inputs"][str(path)] = {
            "sha256": hashlib.sha256(content).hexdigest(), "content": content.decode("utf-8-sig")}
    failed = False
    for name, command in commands.items():
        try:
            result = subprocess.run(command, text=True, encoding="utf-8", errors="replace",
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            entry = {"path": shutil.which(command[0]), "command": command,
                     "exit_code": result.returncode, "output": result.stdout.strip()}
            failed |= result.returncode != 0
        except (OSError, subprocess.TimeoutExpired) as error:
            entry = {"command": command, "error": str(error)}
            failed = True
        report["tools"][name] = entry
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Toolchain report: {args.output}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
