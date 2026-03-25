cmake_minimum_required(VERSION 3.16)
project(renderdoc-tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RenderDoc paths
set(RENDERDOC_DIR "D:/renderdoc" CACHE PATH "Path to RenderDoc source")
set(RENDERDOC_LIB_DIR "${RENDERDOC_DIR}/x64/Release" CACHE PATH "Path to renderdoc.lib/dll")

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(renderdoc-tool ${SOURCES})

target_compile_definitions(renderdoc-tool PRIVATE
    RENDERDOC_PLATFORM_WIN32
)

target_include_directories(renderdoc-tool PRIVATE
    src/include
    third_party
    ${RENDERDOC_DIR}/renderdoc/api/replay
    ${RENDERDOC_DIR}/renderdoc/api  # for sibling includes like app/
)

target_link_libraries(renderdoc-tool PRIVATE
    ${RENDERDOC_LIB_DIR}/renderdoc.lib
)

# Copy renderdoc.dll to output directory
add_custom_command(TARGET renderdoc-tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RENDERDOC_LIB_DIR}/renderdoc.dll"
        $<TARGET_FILE_DIR:renderdoc-tool>
)
