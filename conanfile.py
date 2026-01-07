from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain

class ArchitectureCourseRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    options = {
        "sanitaizer": [True, False],
        "json_rpc_server": [True, False],
        "soap_server": [True, False]
    }
    
    default_options = {
        "sanitaizer": False,
        "json_rpc_server": False,
        "soap_server": False
    }

    def requirements(self):
        self.requires("boost/1.85.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("quill/10.0.1")
        self.requires("tomlplusplus/3.4.0")
        if self.options.soap_server:
            self.requires("pugixml/1.15")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SANITAIZER"] = self.options.sanitaizer
        tc.variables["JSON_RPC_SERVER"] = self.options.json_rpc_server
        tc.variables["SOAP_SERVER"] = self.options.soap_server
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()