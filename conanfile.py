from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class DSSQTConan(ConanFile):
    name = "dss_qt"
    version = "2.0.0"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "with_opencv": [True, False],
    }
    default_options = {
        "with_opencv": True,
        "opencv/*:calib3d": False,
        "opencv/*:dnn": False,
        "opencv/*:features2d": False,
        "opencv/*:flann": False,
        "opencv/*:gapi": False,
        "opencv/*:highgui": False,
        "opencv/*:imgcodecs": False,
        "opencv/*:ml": False,
        "opencv/*:objdetect": False,
        "opencv/*:photo": False,
        "opencv/*:stitching": False,
        "opencv/*:video": False,
        "opencv/*:videoio": False,
        "opencv/*:with_ffmpeg": False,
        "opencv/*:with_flatbuffers": False,
        "opencv/*:with_jpeg": False,
        "opencv/*:with_jpeg2000": False,
        "opencv/*:with_openexr": False,
        "opencv/*:with_png": False,
        "opencv/*:with_protobuf": False,
        "opencv/*:with_quirc": False,
        "opencv/*:with_tiff": False,
        "opencv/*:with_webp": False,
    }

    def requirements(self):
        self.requires("gtest/1.15.0")
        self.requires("spdlog/1.14.0")
        self.requires("nlohmann_json/3.11.3")
        if self.options.with_opencv:
            self.requires("opencv/4.10.0")

    def layout(self):
        cmake_layout(self)
        self.folders.generators = "generators"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
