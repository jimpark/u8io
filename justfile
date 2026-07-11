builddir := "build"
config := "Debug"

# List available recipes
default:
    @just --list

# Run clang-format in-place on all source files
format:
    clang-format -i include/u8io/*.hpp tests/*.cpp tests/*.hpp

# Configure, build, and run the tests
test:
    cmake -S . -B {{builddir}}
    cmake --build {{builddir}} --config {{config}}
    ctest --test-dir {{builddir}} -C {{config}} --output-on-failure

# Install the headers and CMake package (optionally: just install some/prefix/dir)
install prefix="":
    cmake -S . -B {{builddir}}
    cmake --install {{builddir}} --config {{config}} {{ if prefix == "" { "" } else { "--prefix " + quote(prefix) } }}
