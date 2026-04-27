cmake_minimum_required(VERSION 3.20)
project(fepher)

set(CMAKE_CXX_STANDARD 23)

set(SOURCE_FILES
  src/tcp/tcp.cpp
  src/tpool/tpool.cpp
  src/common.cpp
  src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

#target_compile_options(fepher PRIVATE -g)
