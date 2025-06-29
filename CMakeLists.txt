cmake_minimum_required(VERSION 3.10)
project(FlashSale)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 vcpkg 工具链
# 请确保你在命令行执行时加上 -DCMAKE_TOOLCHAIN_FILE=...（看底部说明）

# 查找依赖
find_package(Threads REQUIRED)
find_package(redis++ CONFIG REQUIRED)   # 自动包含 hiredis
find_package(spdlog CONFIG REQUIRED)

# 包含头文件路径（包括当前 src 目录）
include_directories(
    ${PROJECT_SOURCE_DIR}/src
)

# 递归查找 src 下所有 .cpp 文件
file(GLOB_RECURSE SRC
    src/*.cpp
)

# 生成可执行文件
add_executable(flash_sale ${SRC})

# 链接依赖库
target_link_libraries(flash_sale
    Threads::Threads
    redis++::redis++     # ✅ redis++ 会自动链接 hiredis
    spdlog::spdlog       # ✅ spdlog 日志库
)

# 包含 src 作为 include 目录（用于头文件 include 相对路径）
target_include_directories(flash_sale PRIVATE ${PROJECT_SOURCE_DIR}/src)
