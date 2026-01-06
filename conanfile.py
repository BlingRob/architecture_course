from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain

class ArchitectureCourseRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    options = {
        "json_rpc_server": [True, False]
    }
    
    default_options = {
        "json_rpc_server": False
    }

    def requirements(self):
        self.requires("boost/1.85.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("quill/10.0.1")
        self.requires("tomlplusplus/3.4.0")  

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["JSON_RPC_SERVER"] = self.options.json_rpc_server
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()