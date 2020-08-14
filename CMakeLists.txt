cmake_minimum_required(VERSION 3.10)
project(diannex)

option(LINK_LIBSTD_FS "Link with libstdc++fs" OFF)
option(LINK_LIBCPP_FS "Link with libc++fs" OFF)

add_executable(diannex src/main.cpp src/Lexer.cpp src/Parser.cpp src/Bytecode.cpp src/Binary.cpp src/BinaryWriter.cpp src/Utility.cpp src/Translation.cpp src/Context.cpp src/libs/miniz/miniz.c)
target_include_directories(diannex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_property(TARGET diannex PROPERTY CXX_STANDARD 17)

if (LINK_LIBSTD_FS)
    target_link_libraries(diannex stdc++fs)
elseif(LINK_LIBCPP_FS)
    target_link_libraries(diannex c++fs)
endif()
