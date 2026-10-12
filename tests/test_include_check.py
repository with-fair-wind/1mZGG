"""Exercise the header gate with a real Git/Ninja project and clang-tidy."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class IncludeCheckIntegrationTest(unittest.TestCase):
    def test_header_only_warning_fails_gate_and_clean_header_passes(self):
        drivers = ("clang-cl", "clang++") if os.name == "nt" else ("clang++",)
        for driver in drivers:
            with self.subTest(driver=driver):
                self.check_driver(driver)

    def check_driver(self, driver):
        compiler = shutil.which(driver)
        tidy = os.environ.get("DSS_LLVM_BIN")
        tidy = str(Path(tidy) / ("clang-tidy.exe" if os.name == "nt" else "clang-tidy")) if tidy else shutil.which("clang-tidy")
        ninja = shutil.which("ninja")
        for name, tool in (("compiler", compiler), ("clang-tidy", tidy), ("Ninja", ninja), ("CMake", shutil.which("cmake")), ("Git", shutil.which("git"))):
            self.assertTrue(tool and Path(tool).is_file(), f"Required integration tool missing: {name}")
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="include check ", dir=ROOT / "build") as directory:
            source = Path(directory)
            build = source / "build with spaces"
            header = source / "include/dss/header sample.h"
            header.parent.mkdir(parents=True)
            (source / "src").mkdir()
            (source / "tools").mkdir()
            shutil.copyfile(ROOT / "tools/run_include_check.py", source / "tools/run_include_check.py")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.27)\n"
                "project(IncludeCheckProbe LANGUAGES CXX)\n"
                "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
                "add_executable(probe src/probe.cpp)\n"
                "target_compile_features(probe PRIVATE cxx_std_23)\n"
                "target_precompile_headers(probe PRIVATE <atomic>)\n"
                "target_include_directories(probe PRIVATE include)\n", encoding="utf-8")
            (source / "src/probe.cpp").write_text(
                '#include "dss/header sample.h"\nint main() { return answer(); }\n', encoding="utf-8")
            header.write_text("#pragma once\n#include <atomic>\ninline int answer() { return 0; }\n", encoding="utf-8")
            (source / ".clang-tidy").write_text(
                "Checks: '-*,misc-include-cleaner'\nCheckOptions:\n"
                "  misc-include-cleaner.MissingIncludes: false\n", encoding="utf-8")

            def run(arguments):
                return subprocess.run(arguments, cwd=source, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                                      errors="replace", timeout=120)

            def must_pass(arguments):
                result = run(arguments)
                self.assertEqual(result.returncode, 0, result.stdout)
                return result

            must_pass(["git", "init", "--quiet"])
            must_pass(["git", "add", "CMakeLists.txt", ".clang-tidy", "include", "src"])
            must_pass(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                       "-DCMAKE_CXX_COMPILER=" + compiler])
            command = [sys.executable, str(ROOT / "tools/run_include_check.py"),
                       "--source-dir", str(source), "--build-dir", str(build),
                       "--clang-tidy", tidy, "--ninja", ninja, "--warnings-as-errors"]
            # The real CMake gate must fail before expensive source analysis.
            unbuilt = run(["cmake", "-DCLANG_TIDY=" + tidy,
                           "-DSOURCE_DIR=" + str(source), "-DBUILD_DIR=" + str(build),
                           "-DPYTHON_EXECUTABLE=" + sys.executable,
                           "-DNINJA_EXECUTABLE=" + ninja, "-DGENERATOR=Ninja",
                           "-DMODE=check", "-DWARNINGS_AS_ERRORS=ON",
                           "-P", str(ROOT / "cmake/RunClangTidy.cmake")])
            self.assertNotEqual(unbuilt.returncode, 0, unbuilt.stdout)
            self.assertIn("no built Ninja dependencies", unbuilt.stdout)
            self.assertIn("cmake --build", unbuilt.stdout)
            self.assertNotIn("Running full source clang-tidy checks", unbuilt.stdout)
            self.assertNotIn("Processing file", unbuilt.stdout)
            must_pass(["cmake", "--build", str(build)])
            diagnostic = run(command)
            self.assertNotEqual(diagnostic.returncode, 0, diagnostic.stdout)
            self.assertIn("misc-include-cleaner", diagnostic.stdout)
            self.assertIn("header sample.h", diagnostic.stdout)
            report = json.loads((build / "include-check/report.json").read_text(encoding="utf-8"))
            self.assertEqual(len(report["active_headers"]), 1)
            self.assertEqual(len(report["failures"]), 1)
            header.write_text("#pragma once\ninline int answer() { return 0; }\n", encoding="utf-8")
            must_pass(["cmake", "--build", str(build)])
            clean = must_pass(command)
            self.assertIn("0 failed inputs", clean.stdout)


if __name__ == "__main__":
    unittest.main()
