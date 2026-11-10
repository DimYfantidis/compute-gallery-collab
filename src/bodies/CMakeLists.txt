cmake_minimum_required(VERSION 3.28) # 3.1 << C_STANDARD 11

project(bodies LANGUAGES CXX)

find_package(OpenGL REQUIRED)

include(FetchContent)

# ---------------------------------------
# GLFW
# ---------------------------------------

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# ---------------------------------------
# GLAD 2
# ---------------------------------------

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake
)

FetchContent_MakeAvailable(glad)

# Generate OpenGL 3.3 Core bindings
glad_add_library(
    glad_gl_core_33
    STATIC
    REPRODUCIBLE
    API gl:core=3.3
)

# ---------------------------------------
# GLM & stb: use the system copy if there is one (apt: libglm-dev, libstb-dev),
# otherwise fetch them so the build also works on Windows and macOS.
# ---------------------------------------

find_package(glm CONFIG QUIET)

if (NOT TARGET glm::glm)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

find_path(STB_INCLUDE_DIR stb/stb_image.h)

if (NOT STB_INCLUDE_DIR)
    # Sources are placed under .../stb-src/stb/ so that <stb/stb_image.h> resolves
    # exactly as it does with the Debian/Ubuntu package layout.
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG f58f558c120e9b32c217290b80bad1a0729fbb2c
        SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/stb-src/stb
    )
    FetchContent_MakeAvailable(stb)
    set(STB_INCLUDE_DIR ${FETCHCONTENT_BASE_DIR}/stb-src)
endif()

set(SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/textures.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/body_processing.cpp
)

# Application resources
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Copy resources to the executable directory
add_custom_target(copy_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DATA_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Copying application resources..."
)

add_executable(${PROJECT_NAME} ${SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${STB_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE 
    OpenGL::GL 
    glad_gl_core_33
    glfw
    glm::glm
)
set_target_properties(${PROJECT_NAME} PROPERTIES 
    CXX_STANDARD 20 
    CXX_STANDARD_REQUIRED ON 
    CXX_EXTENSIONS OFF
)
target_compile_definitions(${PROJECT_NAME} PRIVATE DATA_DIRECTORY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data" STB_IMAGE_IMPLEMENTATION)
cg_enable_native_arch(${PROJECT_NAME})
