"""Build instrumented core dependencies and run the bounded tracking/concurrency suite.

Run Windows commands in an x64 Visual Studio developer shell with clang-cl on PATH.
The build directory is independent of normal builds. A detected Conan build profile
and a compatible host profile are required; no user profile is modified here.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(args, root, env):
    print(subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    subprocess.run([str(arg) for arg in args], cwd=root, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitizer", choices=("address", "thread"), required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--host-profile", required=True)
    parser.add_argument("--build-profile", default="default")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--qt-prefix", help="Optional installed Qt prefix; includes replay/UI and UDP tests")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    windows = os.name == "nt"
    if windows and args.sanitizer != "address":
        parser.error("Windows supports address only; run thread on Linux")
    root = Path(__file__).resolve().parents[1]
    build = Path(args.build_dir).resolve()
    if not build.is_relative_to(root / "build"):
        parser.error("--build-dir must be under the repository build directory")
    qt = Path(args.qt_prefix).resolve() if args.qt_prefix else None
    if qt and not (qt / "lib/cmake/Qt6/Qt6Config.cmake").is_file():
        parser.error("--qt-prefix must contain lib/cmake/Qt6/Qt6Config.cmake")
    env = os.environ.copy()
    if qt:
        env["PATH"] = str(qt / "bin") + os.pathsep + env["PATH"]
        env.setdefault("QT_QPA_PLATFORM", "offscreen")
    if windows:
        compiler = shutil.which("clang-cl")
        if not compiler:
            parser.error("clang-cl must be available in the Visual Studio developer shell")
        runtime = Path(subprocess.check_output(
            [compiler, "--print-resource-dir"], text=True).strip()) / "lib/windows"
        flags = ["/fsanitize=address"]
        # Conan joins these into CMAKE_EXE_LINKER_FLAGS, a command-line string.
        # Conan embeds this in a quoted CMake string, so escape the linker quotes.
        link = [r'\"' + (runtime / "clang_rt.asan_dynamic-x86_64.lib").as_posix() + r'\"',
                "/INCLUDE:__asan_seh_interceptor",
                r'/WHOLEARCHIVE:\"' + (runtime / "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib").as_posix() + r'\"']
        env["PATH"] = str(runtime) + os.pathsep + env["PATH"]
    else:
        flags = ["-fsanitize=" + args.sanitizer, "-fno-omit-frame-pointer"]
        link = ["-fsanitize=" + args.sanitizer]
        env["TSAN_OPTIONS"] = "halt_on_error=1"
    install = ["conan", "install", ".", "-pr:h", args.host_profile,
               "-pr:b", args.build_profile, "-s:h", "build_type=Release",
               "-s:h", "compiler.cppstd=23", "-o", "&:with_opencv=False",
               "-c:h=tools.build:cxxflags=" + json.dumps(flags),
               "-c:h=tools.build:exelinkflags=" + json.dumps(link),
               '-c:h=tools.info.package_id:confs=["tools.build:cxxflags"]',
               '-c:h=tools.cmake.cmaketoolchain:extra_variables={"CMAKE_TRY_COMPILE_CONFIGURATION":"Release"}',
               "--output-folder=" + str(build), "--build=missing"]
    if windows:
        # Conan's vcvars activation can put Visual Studio's older Clang first.
        # Bind dependencies and the project to the compiler owning this runtime.
        install.append("-c:h=tools.build:compiler_executables=" +
                       json.dumps({"c": compiler, "cpp": compiler}))
    if args.offline:
        install.append("--no-remote")
    run(install, root, env)
    configure = ["cmake", "--fresh", "-S", root, "-B", build, "-G", "Ninja",
         "-DCMAKE_TOOLCHAIN_FILE=" + (build / "generators/conan_toolchain.cmake").as_posix(),
         "-DCMAKE_BUILD_TYPE=Release", "-DDSS_BUILD_APP=" + ("ON" if qt else "OFF"), "-DDSS_ENABLE_OPENCV=OFF",
         "-DDSS_ENABLE_TESTS=ON", "-DDSS_ENABLE_CUDA=OFF", "-DDSS_ENABLE_SAPERA=OFF",
         "-DDSS_ENABLE_COLOR_DIAGNOSTICS=OFF", "-DDSS_SANITIZER=" + args.sanitizer]
    if qt:
        configure.append("-DCMAKE_PREFIX_PATH=" + qt.as_posix())
    run(configure, root, env)
    targets = ["test_bounded_channel", "test_async_write_queue", "test_image_processor",
               "test_geo_tracker", "test_leo_tracker", "test_sc_tracker", "test_manual_tracker",
               "test_tracking_candidate_utils", "test_tracking_lifecycle_utils",
               "test_tracking_prediction_utils", "test_track_data_storage_backend",
               "test_track_result_data_exchange_bridge"]
    if qt:
        targets += ["test_replay_session", "test_replay_view_model", "test_main_view_model", "test_display_view_model",
                    "test_udp_channel", "test_data_exchange", "test_image_sender"]
    run(["cmake", "--build", build, "--parallel", args.jobs, "--target", *targets], root, env)
    pattern = (r"^(BoundedChannel(Test)?|AsyncWriteQueue|ImageProcessor|GeoTracker|LeoTracker|"
               r"ScTracker|ManualTracker|TrackingCandidateUtils|TrackingLifecycleUtils|"
               r"TrackingPredictionUtils|TrackDataStorageBackend|TrackResultDataExchangeBridge)\.")
    if qt:
        pattern += (r"|^(test_replay_session|test_replay_view_model|test_main_view_model|"
                    r"test_display_view_model|test_udp_channel)$|^(DataExchange|ImageSender|UdpChannel)\.")
    run(["ctest", "--test-dir", build, "--output-on-failure", "--no-tests=error",
         "--timeout", "90", "--output-junit", "sanitizer-results.xml", "-R", pattern], root, env)


if __name__ == "__main__":
    main()
