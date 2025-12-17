cmake_minimum_required(VERSION 3.1)
project(StableDiffusionNCNN)

set(DEBUG                   true)
set(OPTIMIZATION_LEVEL      0)

set(IS_SHOW_IMAGE true)

set(BIN_NAME "stable-diffusion-ncnn")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

SET(CMAKE_PREFIX_PATH ncnn-20230517-ubuntu-2204/lib/cmake/ncnn)

FIND_PACKAGE(ncnn REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/third_party)

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

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O${OPTIMIZATION_LEVEL}")

IF(DEBUG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    ADD_COMPILE_DEFINITIONS(DEBUG)
ENDIF()

ADD_EXECUTABLE(${BIN_NAME} ${CORE_SOURCES} ${DEMO_SOURCES})
ADD_EXECUTABLE(${BIN_NAME}-mcp ${CORE_SOURCES} ${MCP_SOURCES})

TARGET_LINK_LIBRARIES(${BIN_NAME} ncnn)
TARGET_LINK_LIBRARIES(${BIN_NAME}-mcp ncnn)

FILE(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
