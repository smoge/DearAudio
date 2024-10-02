# Justfile for DearAudio project

# Default recipe to run when just is called without arguments
default:
    @just --list

# Build the project
build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

# Build the project with Debug
debug:
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build

# Clean build artifacts
clean:
    rm -rf build

# Run the application
run:
    ./build/DearAudio

# Generate compile_commands.json
gen-compile-commands:
    cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ln -sf build/compile_commands.json .

# Update dependencies 
# update-deps:
#     git submodule update --init --recursive

# Run tests (if you add tests in the future)
test:
    cd build && ctest

# Install the application
# install:
#     cmake --install build

# Format code (assuming you're using clang-format)
format:
    find . -iname *.hpp -o -iname *.cpp | xargs clang-format -i

# Static analysis (assuming you're using clang-tidy)
lint:
    find . -iname *.cpp | xargs clang-tidy
