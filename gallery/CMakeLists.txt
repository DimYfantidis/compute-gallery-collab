# The gallery: a Dear ImGui launcher that reads demos/*/demo.json at runtime and starts
# each demo as its own program. Demos need no changes to appear here, only a manifest.

include(FetchContent)

if (WIN32)
    enable_language(RC) # the executable's icon
endif()
# Set by the top-level CMakeLists for packages; plain defaults otherwise.
if (NOT DEFINED CG_INSTALL_ROOT)
    set(CG_INSTALL_ROOT .)
endif()
if (NOT DEFINED CG_MACOS_BUNDLE)
    set(CG_MACOS_BUNDLE OFF)
endif()

# ---------------------------------------
# GLFW + GLAD (reuse the targets the bodies demo already declared)
# ---------------------------------------

if (NOT TARGET glfw)
    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.5.1 GIT_SHALLOW TRUE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

if (NOT TARGET glad_gl_core_33)
    FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v2.0.8 SOURCE_SUBDIR cmake)
    FetchContent_MakeAvailable(glad)
    glad_add_library(glad_gl_core_33 STATIC REPRODUCIBLE API gl:core=3.3)
endif()

# ---------------------------------------
# Dear ImGui, nlohmann/json, OpenCL headers, stb
# ---------------------------------------

FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.9b GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
# The gallery's standard; Apple's clang would otherwise compile it as C++98.
set_target_properties(imgui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(nlohmann_json)

# Only the headers: the gallery loads the OpenCL library at runtime.
find_path(OPENCL_HEADERS_DIR CL/cl.h)
if (NOT OPENCL_HEADERS_DIR)
    # SOURCE_SUBDIR points at nothing so only the headers are downloaded, not built or installed.
    FetchContent_Declare(opencl_headers GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
                         GIT_TAG v2025.07.22 GIT_SHALLOW TRUE SOURCE_SUBDIR headers-only)
    FetchContent_MakeAvailable(opencl_headers)
    set(OPENCL_HEADERS_DIR ${opencl_headers_SOURCE_DIR})
endif()

find_path(STB_INCLUDE_DIR stb/stb_image.h)
if (NOT STB_INCLUDE_DIR)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG f58f558c120e9b32c217290b80bad1a0729fbb2c
        SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/stb-src/stb
    )
    FetchContent_MakeAvailable(stb)
    set(STB_INCLUDE_DIR ${FETCHCONTENT_BASE_DIR}/stb-src)
endif()

# ---------------------------------------
# The gallery executable, placed at the root of the build/install tree so that the
# manifests' relative paths (e.g. "bodies/bodies") resolve next to it.
# ---------------------------------------

set(ICON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon)
# Inter (SIL OFL 1.1) and the Lucide icon font (ISC), with their licenses.
set(GALLERY_FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-Medium.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-SemiBold.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/InterDisplay-Bold.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-LICENSE.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/lucide.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/lucide-LICENSE.txt
    ${imgui_SOURCE_DIR}/misc/fonts/Cousine-Regular.ttf
)

set(WINDOW_ICONS
    ${ICON_DIR}/compute-gallery-16.png
    ${ICON_DIR}/compute-gallery-32.png
    ${ICON_DIR}/compute-gallery-48.png
    ${ICON_DIR}/compute-gallery-256.png
)

add_executable(compute-gallery WIN32
    src/main.cpp
    src/app.cpp
    src/background.cpp
    src/catalog.cpp
    src/embed.cpp
    src/logo.cpp
    src/options.cpp
    src/pages.cpp
    src/process.cpp
    src/runner.cpp
    src/settings.cpp
    src/stage.cpp
    src/system_info.cpp
    src/textures.cpp
    src/theme.cpp
    src/widgets.cpp
)
target_compile_definitions(compute-gallery PRIVATE CG_VERSION="${PROJECT_VERSION}")
if (UNIX AND NOT APPLE)
    # Demo windows are embedded with Xlib on X11.
    find_package(X11 REQUIRED)
    target_link_libraries(compute-gallery PRIVATE X11::X11)
endif()
if (WIN32)
    target_sources(compute-gallery PRIVATE ${ICON_DIR}/compute-gallery.rc)
endif()
if (CG_MACOS_BUNDLE)
    # Packaged for macOS as "Compute Gallery.app"; the rest of the package goes into its
    # Contents/Resources (CG_INSTALL_ROOT), where the gallery looks for it.
    target_sources(compute-gallery PRIVATE ${ICON_DIR}/compute-gallery.icns)
    set_source_files_properties(${ICON_DIR}/compute-gallery.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(compute-gallery PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Compute Gallery"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME "Compute Gallery"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.compute-gallery"
        MACOSX_BUNDLE_ICON_FILE compute-gallery.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()
target_include_directories(compute-gallery SYSTEM PRIVATE
    ${OPENCL_HEADERS_DIR}
    ${STB_INCLUDE_DIR}
)
target_link_libraries(compute-gallery PRIVATE imgui glad_gl_core_33 nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})
set_target_properties(compute-gallery PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
if (MSVC)
    # WIN32 subsystem, but keep the portable main() entry point.
    target_link_options(compute-gallery PRIVATE /ENTRY:mainCRTStartup)
endif()
find_package(Threads REQUIRED)
target_link_libraries(compute-gallery PRIVATE Threads::Threads)

# Manifests, thumbnails, fonts and icons next to the executable, refreshed on every build so
# a new or edited demo folder shows up without touching the gallery's code.
add_custom_target(gallery_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/demos ${CMAKE_BINARY_DIR}/demos
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets/fonts ${CMAKE_BINARY_DIR}/assets/icons
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GALLERY_FONTS} ${CMAKE_BINARY_DIR}/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WINDOW_ICONS} ${CMAKE_BINARY_DIR}/assets/icons
    COMMENT "Copying gallery manifests, fonts and icons..."
)
add_dependencies(compute-gallery gallery_assets)

install(TARGETS compute-gallery RUNTIME DESTINATION ${CG_INSTALL_ROOT} BUNDLE DESTINATION .)
install(DIRECTORY demos DESTINATION ${CG_INSTALL_ROOT})
install(FILES ${GALLERY_FONTS} DESTINATION ${CG_INSTALL_ROOT}/assets/fonts)
install(FILES ${WINDOW_ICONS} DESTINATION ${CG_INSTALL_ROOT}/assets/icons)

# ---------------------------------------
# Tests for the launcher, manifests and options (no window needed): ctest, or gallery_tests.
# ---------------------------------------

add_executable(gallery_tests
    tests/gallery_tests.cpp
    src/catalog.cpp
    src/options.cpp
    src/process.cpp
    src/runner.cpp
)
target_include_directories(gallery_tests PRIVATE src)
target_link_libraries(gallery_tests PRIVATE nlohmann_json::nlohmann_json Threads::Threads)
set_target_properties(gallery_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME gallery COMMAND gallery_tests)
