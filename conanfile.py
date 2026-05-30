from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class DSSQTConan(ConanFile):
    name = "dss_qt"
    version = "2.0.0"
    settings = "os", "compiler", "build_type", "arch"

    options = {"with_conan_qt": [True, False]}
    default_options = {"with_conan_qt": False}

    def requirements(self):
        if self.options.with_conan_qt:
            self.requires("qt/6.8.0")
        self.requires("gtest/1.15.0")
        self.requires("spdlog/1.14.0")
        self.requires("nlohmann_json/3.11.3")

    def configure(self):
        if self.options.with_conan_qt:
            self.options["qt/*"].shared = True
            self.options["qt/*"].qtcharts = True
            self.options["qt/*"].qtserialport = True

    def layout(self):
        cmake_layout(self, build_folder="build")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
