cmake_minimum_required(VERSION 3.15)
project(StableDiffusionNCNN LANGUAGES CXX)

set(BIN_NAME "stable-diffusion-ncnn")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SD_BUILD_DEMO "Build file-based demo executable" ON)
option(SD_BUILD_MCP "Build MCP stdio server executable" ON)

find_package(ncnn CONFIG REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party)

set(CORE_SOURCES
    src/decoder_slover.cpp
    src/diffusion_slover.cpp
    src/prompt_slover.cpp
    src/stb_image_write_impl.cpp
)

set(DEMO_SOURCES
    src/opencv-mobile_ncnn-demo.cpp
)

set(MCP_SOURCES
    src/mcp_stdio_server.cpp
)

if (SD_BUILD_DEMO)
    add_executable(${BIN_NAME} ${CORE_SOURCES} ${DEMO_SOURCES})
    target_link_libraries(${BIN_NAME} PRIVATE ncnn)
    if (WIN32)
        target_link_libraries(${BIN_NAME} PRIVATE psapi)
    endif()
endif()

if (SD_BUILD_MCP)
    add_executable(${BIN_NAME}-mcp ${CORE_SOURCES} ${MCP_SOURCES})
    target_link_libraries(${BIN_NAME}-mcp PRIVATE ncnn)
endif()

FILE(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
