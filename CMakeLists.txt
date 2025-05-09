cmake_minimum_required(VERSION 4.0)

project(micro)

set(SOURCE_EXE src/main.c)

set(SOURCE_LIB src/buffer.c src/row.c src/color.c src/terminal.c src/finder.c src/editor.c)

add_library(editor STATIC ${SOURCE_LIB})

add_executable(micro src/main.c)

target_link_libraries(micro editor)
