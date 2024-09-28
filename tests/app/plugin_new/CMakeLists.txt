
cmake_minimum_required(VERSION 3.10)
project(plugin_new)

# 设置插件输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/plugin)

# 设置插件头文件目录（根据实际路径调整）
set(PLUGIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

# 包含插件的头文件目录
include_directories(${PLUGIN_INCLUDE_DIR})

# 包含 EmbedMetadata.cmake 脚本，用于嵌入元数据
include(${PLUGIN_INCLUDE_DIR}/EmbedMetadata.cmake)

# 设置隐藏符号可见性，避免不同插件间的符号冲突
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

# 在 Windows 上需要设置运行时输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# 自动创建 pluginMetadata 目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pluginMetadata)

# 嵌入元数据，使用绝对路径引用 meta.json
embed_metadata(
    METADATA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/meta.json
    HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/pluginMetadata/metadata.h
    VARIABLE_NAME "jp_metadata"
    NULL_TERMINATE ON
)

# 添加生成的 pluginMetadata 目录到包含目录，以便在代码中引用 metadata.h
include_directories(${CMAKE_CURRENT_BINARY_DIR}/pluginMetadata)

# 添加共享库目标，包含插件源文件和生成的 metadata.h
add_library(${PROJECT_NAME} SHARED plugin_new.cpp )
