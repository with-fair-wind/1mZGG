"""Check active project headers as primary files using their built Ninja consumers.

The compilation database supplies all defines, language and include paths. Ninja
supplies the actual header dependencies; optional inactive headers are reported.
PCH and compiler output flags are removed from the derived, temporary database.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys


SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx"}
HEADER_SUFFIXES = {".h", ".hpp", ".hxx"}


def path_key(value, directory):
    path = Path(value)
    if not path.is_absolute():
        path = directory / path
    return os.path.normcase(str(path.resolve()))


def in_scope(path, root):
    if not path.is_relative_to(root):
        return False
    parts = path.relative_to(root).parts
    return parts[0] in {"src", "tests", "tools", "benchmarks"} or parts[:2] == ("include", "dss")


def command_arguments(entry):
    if "arguments" in entry:
        return entry["arguments"][:]
    if os.name != "nt":
        return shlex.split(entry["command"])
    shell = ctypes.WinDLL("shell32")
    shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
    count = ctypes.c_int()
    pointer = shell.CommandLineToArgvW(entry["command"], ctypes.byref(count))
    if not pointer:
        raise OSError("Cannot parse compiler command")
    try:
        return [pointer[i] for i in range(count.value)]
    finally:
        free = ctypes.WinDLL("kernel32").LocalFree
        free.argtypes = [ctypes.c_void_p]
        free.restype = ctypes.c_void_p
        free(ctypes.cast(pointer, ctypes.c_void_p))


def header_arguments(entry, header):
    original = command_arguments(entry)
    # Clang's GNU driver forwards PCH options as paired -Xclang arguments.
    filtered = []
    index = 0
    while index < len(original):
        if (index + 3 < len(original) and original[index] == "-Xclang"
                and original[index + 1] in {"-include-pch", "-include"}
                and original[index + 2] == "-Xclang"
                and (original[index + 1] == "-include-pch"
                     or "cmake_pch" in original[index + 3])):
            index += 4
            continue
        filtered.append(original[index])
        index += 1
    original = filtered
    directory = Path(entry["directory"])
    source = path_key(entry["file"], directory)
    result = [original[0]]
    skip = False
    replaced = 0
    for index, argument in enumerate(original[1:], 1):
        if skip:
            skip = False
            continue
        if argument in {"-o", "-MF", "-MT", "-MQ", "-include-pch"}:
            skip = True
            continue
        if argument in {"-MD", "-MMD", "/clang:-MD", "/clang:-MMD"}:
            continue
        if argument.startswith(("/Fo", "/Fd", "/Fp", "/Yu", "/Yc", "-Fo", "-Fd", "-Fp", "-Yu", "-Yc", "/clang:-MF", "/clang:-MT")):
            continue
        if argument in {"-include", "/FI", "-FI"} and index + 1 < len(original):
            if "cmake_pch" in original[index + 1]:
                skip = True
                continue
        if argument.startswith(("/FI", "-FI")) and "cmake_pch" in argument:
            continue
        if path_key(argument, directory) == source:
            result.append(str(header))
            replaced += 1
        else:
            result.append(argument)
    if replaced != 1:
        raise RuntimeError(f"Expected one source argument for {entry['file']}, found {replaced}")
    # GCC drivers infer header language from its suffix; explicitly parse C++.
    if "clang-cl" not in Path(original[0]).name.lower():
        result[1:1] = ["-x", "c++"]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--clang-tidy", required=True)
    parser.add_argument("--ninja", required=True)
    parser.add_argument("--mode", choices=("check", "fix"), default="check")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--warnings-as-errors", action="store_true")
    parser.add_argument("--sources", action="store_true", help="Also scan source inputs without PCH (development audit)")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    root, build = args.source_dir.resolve(), args.build_dir.resolve()
    build_hint = f'Run: cmake --build "{build}" --parallel 4'
    out = build / "include-check"
    out.mkdir(parents=True, exist_ok=True)
    entries = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    for entry in entries:
        entry["file"] = path_key(entry["file"], Path(entry["directory"]))
    entries = [e for e in entries if in_scope(Path(e["file"]), root)
               and Path(e["file"]).suffix in SOURCE_SUFFIXES]
    if not entries:
        raise RuntimeError("No project source compile commands; configure and build first")
    tracked = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"], encoding="utf-8").split("\0")
    headers = {path_key(root / name, root): root / name for name in tracked
               if name and in_scope(root / name, root) and Path(name).suffix in HEADER_SUFFIXES}
    output_entries = {}
    for entry in entries:
        if "output" not in entry:
            raise RuntimeError("Header checks require compilation database output fields")
        output_entries.setdefault(path_key(entry["output"], Path(entry["directory"])), []).append(entry)
    deps = subprocess.check_output([args.ninja, "-C", str(build), "-t", "deps"], encoding="utf-8", errors="replace")
    consumers = {}
    seen_outputs = set()
    current = []
    for line in deps.splitlines():
        match = re.match(r"^(.*): #deps .*", line)
        if match:
            key = path_key(match.group(1), build)
            current = output_entries.get(key, [])
            if current:
                if "(VALID)" not in line:
                    raise RuntimeError(f"Stale dependency inventory: {match.group(1)}. {build_hint}")
                seen_outputs.add(key)
        elif current and line.startswith("    "):
            key = path_key(line.strip(), build)
            if key in headers:
                consumers.setdefault(key, []).extend(current)
    missing = set(output_entries) - seen_outputs
    if missing:
        raise RuntimeError(f"{len(missing)} source objects have no built Ninja dependencies; build all default targets first. {build_hint}")
    if not consumers:
        raise RuntimeError("No active tracked project headers found in built dependencies")
    skipped = sorted(str(headers[k].relative_to(root)) for k in headers.keys() - consumers.keys())
    commands = []
    for key, candidates in sorted(consumers.items()):
        header = headers[key]
        relative = header.relative_to(root)
        component = relative.parts[2] if relative.parts[:2] == ("include", "dss") else relative.parts[0]
        def rank(entry):
            source = Path(entry["file"])
            return (source.stem != header.stem, f"/src/{component}/" not in source.as_posix(), source.relative_to(root).parts[0] != "src", len(entry.get("command", "")), str(source))
        entry = min(candidates, key=rank)
        commands.append({"directory": entry["directory"], "file": str(header), "arguments": header_arguments(entry, header)})
    if args.sources:
        unique = {e["file"]: e for e in entries}
        commands.extend({"directory": e["directory"], "file": e["file"], "arguments": header_arguments(e, Path(e["file"]))} for e in unique.values())
    (out / "compile_commands.json").write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
    report = {"active_headers": sorted(str(headers[k].relative_to(root)) for k in consumers), "inactive_headers": skipped, "failures": []}
    print(f"Include Cleaner: {len(consumers)} active headers as primary files", flush=True)
    if skipped:
        print("Inactive headers (not checked in this configuration): " + ", ".join(skipped), flush=True)
    def check(entry):
        token = hashlib.sha256(entry["file"].encode()).hexdigest()[:16]
        cmd = [args.clang_tidy, "-p", str(out), entry["file"], "-checks=-*,misc-include-cleaner", "--use-color=false", "--quiet", "--export-fixes=" + str(out / (token + ".yaml"))]
        if args.warnings_as_errors:
            cmd.append("--warnings-as-errors=*")
        if args.mode == "fix":
            cmd.append("--fix")
        result = subprocess.run(cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=180)
        (out / (token + ".log")).write_text(result.stdout, encoding="utf-8")
        return entry["file"], result.returncode, result.stdout
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for count, (file, code, log) in enumerate(pool.map(check, commands), 1):
            if code or re.search(r"\b(warning|error):", log):
                print(log.rstrip(), flush=True)
            if code:
                report["failures"].append({"file": file, "exit_code": code})
            if count % 20 == 0 or count == len(commands):
                print(f"Checked {count}/{len(commands)} primary files", flush=True)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Include Cleaner completed: {len(report['failures'])} failed inputs", flush=True)
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    sys.exit(main())
