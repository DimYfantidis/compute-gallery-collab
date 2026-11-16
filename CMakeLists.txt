cmake_minimum_required(VERSION 3.28) # 3.1 << C_STANDARD 11

project(compute-gallery VERSION 0.1.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(NativeArch)

enable_testing() # before any add_test, here or in a subdirectory

# Each exhibit is optional so that a host missing one toolchain (e.g. no OpenCL
# ICD on macOS, no SDL2 on a fresh Windows box) can still build the others.
option(CG_BUILD_BODIES "Build the OpenGL bodies renderer" ON)
option(CG_BUILD_OPENCL "Build the OpenCL/SDL2 exhibits and tests" ON)
option(CG_BUILD_GALLERY "Build the gallery launcher" ON)
# Release packages: programs find their data next to themselves instead of in the source
# tree, so `cmake --install` produces a folder that runs on any machine.
option(CG_RELOCATABLE "Look up data relative to the executables (for packaged releases)" OFF)

# Dependencies fetched by the demos must not end up in the installed package.
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Where `cmake --install` puts the gallery's files. A macOS package is an .app bundle with
# everything but the gallery program in Contents/Resources.
if (APPLE AND CG_RELOCATABLE)
    set(CG_MACOS_BUNDLE ON)
    set(CG_INSTALL_ROOT "Compute Gallery.app/Contents/Resources")
else()
    set(CG_MACOS_BUNDLE OFF)
    set(CG_INSTALL_ROOT .)
endif()

if (MINGW)
    # Ship executables that do not need the MinGW C/C++ runtime DLLs.
    add_link_options(-static-libgcc -static-libstdc++)
endif()
if (CG_RELOCATABLE AND NOT MSVC)
    # __FILE__ in error messages: print paths relative to the repo, not the build machine's.
    add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=)
endif()

if (CG_BUILD_BODIES OR CG_BUILD_GALLERY)
    # Both generate their OpenGL loader with GLAD, a Python program that needs Jinja2. Check
    # for it now, with install hints, rather than fail halfway through the build.
    if (MINGW)
        set(Python_FIND_REGISTRY LAST) # in an MSYS2 shell, its own Python before one installed in Windows
    endif()
    find_package(Python COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND "${Python_EXECUTABLE}" -c "import jinja2" RESULT_VARIABLE _jinja2 OUTPUT_QUIET ERROR_QUIET)
    if (NOT _jinja2 EQUAL 0)
        message(FATAL_ERROR "GLAD needs the Python package Jinja2, which ${Python_EXECUTABLE} does not have. Install "
                            "python3-jinja2 (Ubuntu) or mingw-w64-ucrt-x86_64-python-jinja (MSYS2), or run "
                            "pip install jinja2 in a virtual environment (macOS); see \"Building natively\" in README.md.")
    endif()
endif()

if (CG_BUILD_BODIES)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/common "${CMAKE_BINARY_DIR}/common")
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/bodies "${CMAKE_BINARY_DIR}/bodies")

    if (CG_RELOCATABLE)
        # bodies reads DATA_DIRECTORY_PATH (the source folder by default); make it the "data"
        # folder next to the executable, which the gallery uses as working directory.
        get_target_property(_bodies_defs bodies COMPILE_DEFINITIONS)
        list(TRANSFORM _bodies_defs REPLACE "^DATA_DIRECTORY_PATH=.*$" "DATA_DIRECTORY_PATH=\"data\"")
        set_target_properties(bodies PROPERTIES COMPILE_DEFINITIONS "${_bodies_defs}")
    endif()
    install(TARGETS bodies DESTINATION ${CG_INSTALL_ROOT}/bodies)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/bodies/data/ DESTINATION ${CG_INSTALL_ROOT}/bodies/data)
endif()

if (CG_BUILD_OPENCL)
    find_package(OpenCL)
    find_package(SDL2 CONFIG)
    find_package(SDL2_image CONFIG)

    if (OpenCL_FOUND AND SDL2_FOUND AND SDL2_image_FOUND)
        if (CG_RELOCATABLE)
            set(_root_path ".")
        else()
            set(_root_path "${CMAKE_SOURCE_DIR}")
        endif()

        add_library(common STATIC ${CMAKE_SOURCE_DIR}/src/common.c)
        target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)
        if (WIN32 AND TARGET SDL2::SDL2main)
            # SDL.h renames main() to SDL_main() on Windows; SDL2main provides the real entry point.
            # It calls into SDL2, so it goes before SDL2 on the link line.
            target_link_libraries(common PUBLIC SDL2::SDL2main)
        endif()
        target_link_libraries(common PUBLIC OpenCL::OpenCL SDL2::SDL2 SDL2_image::SDL2_image)
        if (NOT MSVC)
            target_link_libraries(common PUBLIC m)
        endif()
        if (MINGW)
            # printf with POSIX extensions (e.g. the ' grouping flag used by test_opencl).
            target_compile_definitions(common PUBLIC __USE_MINGW_ANSI_STDIO=1)
        endif()
        target_compile_definitions(common PUBLIC CL_TARGET_OPENCL_VERSION=300 PROJECT_ROOT_PATH="${_root_path}")

        add_executable(grayscale_cpu ${CMAKE_SOURCE_DIR}/src/grayscale_cpu.c)
        add_executable(grayscale_gpu ${CMAKE_SOURCE_DIR}/src/grayscale_gpu.c)
        add_executable(test_sdl ${CMAKE_SOURCE_DIR}/tests/test_sdl.c)
        add_executable(test_opencl ${CMAKE_SOURCE_DIR}/tests/test_opencl.c)

        foreach (tg IN ITEMS common grayscale_cpu grayscale_gpu test_sdl test_opencl)
            if (NOT tg STREQUAL "common")
                target_link_libraries(${tg} PRIVATE common)
            endif()
            set_target_properties(${tg} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
            cg_enable_native_arch(${tg})
        endforeach()

        # The benchmark checks the OpenCL results against plain C and exits with 1 on a mismatch.
        # It needs an OpenCL device (PoCL is enough); `ctest -LE opencl` skips it where there is none.
        add_test(NAME opencl_benchmark COMMAND test_opencl)
        set_tests_properties(opencl_benchmark PROPERTIES LABELS opencl)

        # test_sdl stays out of packages: it is a smoke test and its image is not ours to ship.
        install(TARGETS grayscale_cpu grayscale_gpu test_opencl DESTINATION ${CG_INSTALL_ROOT})
        install(FILES ${CMAKE_SOURCE_DIR}/data/images/felix-mittermeier.jpg DESTINATION ${CG_INSTALL_ROOT}/data/images)
    else()
        message(WARNING "OpenCL, SDL2 or SDL2_image not found: skipping the OpenCL exhibits")
    endif()
endif()

if (CG_BUILD_GALLERY)
    add_subdirectory(${CMAKE_SOURCE_DIR}/gallery "${CMAKE_BINARY_DIR}/gallery")
endif()

install(FILES README.md THIRD_PARTY_NOTICES.md DESTINATION .)
