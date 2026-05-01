cmake_minimum_required(VERSION 3.14)
project(NoodlesCore VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Third-party dependencies fetched automatically if not found.
include(FetchContent)

# GLEW — OpenGL type definitions and extension loading.
# Build from source as a static library (glew.c is a single file).
find_package(GLEW QUIET)
if(NOT GLEW_FOUND)
    FetchContent_Declare(
        glew
        URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(glew)
    if(NOT glew_POPULATED)
        FetchContent_Populate(glew)
        add_library(glew_static STATIC ${glew_SOURCE_DIR}/src/glew.c)
        target_include_directories(glew_static PUBLIC ${glew_SOURCE_DIR}/include)
        target_compile_definitions(glew_static PUBLIC GLEW_STATIC)
        target_link_libraries(glew_static PUBLIC OpenGL::GL)
        add_library(GLEW::GLEW ALIAS glew_static)
        # Install GLEW headers so downstream consumers can find GL/glew.h
        install(DIRECTORY ${glew_SOURCE_DIR}/include/GL
            DESTINATION include)
    endif()
endif()

# stb — single-file image loading (stb_image.h).
# NOTE: stb has no tagged releases.  Pin to a full 40-char commit SHA
# (short SHAs cannot be resolved by `git fetch` and GIT_SHALLOW only
# works with branch/tag names, not arbitrary commits).
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# rapidjson — JSON parsing (header-only).
find_package(RapidJSON QUIET)
if(NOT RapidJSON_FOUND)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG v1.1.0
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(rapidjson)
    if(NOT rapidjson_POPULATED)
        FetchContent_Populate(rapidjson)
    endif()
endif()

find_package(OpenGL REQUIRED)

set(NOODLES_HEADERS
    core/Animator.h
    core/Glyph.h
    core/LinkSelectionMode.h
    core/Math.h
    core/NodeData.h
    core/NodeVertex.h
    core/RenderConfig.h
    core/api.h
    core/pragmas.h
    render/FontAtlas.h
    render/GraphNodeRenderer.h
    render/GraphRenderer.h
    render/LinkGeometry.h
    render/LinkRenderManager.h
    render/NodeRenderManager.h
    render/NodeVertexCache.h
    render/Profiler.h
    render/ShaderLibrary.h
    render/StickerRenderer.h
    render/TextLayout.h
    render/TextRenderManager.h
    render/VertexGenerator.h
    spatial/SpatialIndex.h
    undo/Command.h
    undo/CompoundCommand.h
    undo/LambdaCommand.h
    undo/NoodlesUndoManager.h
)

set(NOODLES_SOURCES
    core/Animator.cpp
    core/NodeData.cpp
    core/NodeVertex.cpp
    core/RenderConfig.cpp
    render/FontAtlas.cpp
    render/GraphNodeRenderer.cpp
    render/GraphRenderer.cpp
    render/LinkGeometry.cpp
    render/LinkRenderManager.cpp
    render/NodeRenderManager.cpp
    render/NodeVertexCache.cpp
    render/Profiler.cpp
    render/ShaderLibrary.cpp
    render/StickerRenderer.cpp
    render/TextLayout.cpp
    render/TextRenderManager.cpp
    render/VertexGenerator.cpp
    spatial/SpatialIndex.cpp
    undo/CompoundCommand.cpp
    undo/LambdaCommand.cpp
    undo/NoodlesUndoManager.cpp
)

add_library(noodles SHARED ${NOODLES_SOURCES})

# stb_image is header-only; exactly one TU must provide the implementation.
set_source_files_properties(render/FontAtlas.cpp PROPERTIES
    COMPILE_DEFINITIONS "STB_IMAGE_IMPLEMENTATION")

target_include_directories(noodles
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/noodles>
    PRIVATE
        ${stb_SOURCE_DIR}
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(noodles PUBLIC OpenGL::GL GLEW::GLEW)

target_compile_definitions(noodles PRIVATE NOODLES_EXPORTS)

# Install library
install(TARGETS noodles
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers preserving directory structure
install(DIRECTORY core/ DESTINATION include/noodles/core FILES_MATCHING PATTERN "*.h")
install(DIRECTORY render/ DESTINATION include/noodles/render FILES_MATCHING PATTERN "*.h")
install(DIRECTORY spatial/ DESTINATION include/noodles/spatial FILES_MATCHING PATTERN "*.h")
install(DIRECTORY undo/ DESTINATION include/noodles/undo FILES_MATCHING PATTERN "*.h")

# Install resources (fonts, shaders)
install(DIRECTORY assets/ DESTINATION share/noodles/assets)

# Tests (optional — requires GTest)
find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(noodles_tests
        tests/NoodlesCoreTest.cpp
        tests/NoodlesHeadlessTest.cpp
        tests/NoodlesRenderTest.cpp
        tests/NoodlesSpatialTest.cpp
        tests/NoodlesUndoTest.cpp
        tests/NoodlesUsdPrimRegistryTest.cpp
    )
    target_link_libraries(noodles_tests PRIVATE noodles GTest::gtest GTest::gtest_main)
    add_test(NAME noodles_tests COMMAND noodles_tests)
endif()
