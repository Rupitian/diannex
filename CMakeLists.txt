cmake_minimum_required(VERSION 3.16)
project(diannex)

add_executable(diannex src/main.cpp src/Lexer.cpp src/Parser.cpp src/Bytecode.cpp src/Binary.cpp src/BinaryWriter.cpp src/Utility.cpp src/Translation.cpp src/libs/miniz/miniz.c)
target_include_directories(diannex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_property(TARGET diannex PROPERTY CXX_STANDARD 17)
