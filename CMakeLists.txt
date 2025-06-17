cmake_minimum_required(VERSION 3.16)

project(EECS482_P3)
set(CMAKE_CXX_STANDARD 17)

add_library(libpager STATIC IMPORTED)
add_library(libapp STATIC IMPORTED)

if (APPLE)
    add_compile_options(-D_XOPEN_SOURCE)
    set_target_properties(libpager PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libvm_pager_macos.o)
    set_target_properties(libapp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libvm_app_macos.o)
else ()
    set_target_properties(libpager PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libvm_pager.o)
    set_target_properties(libapp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libvm_app.o)
endif ()

# Pager build target (customize this with the source files for your pager)
add_executable(pager file1.cpp file2.cpp)
target_link_libraries(pager libpager)
add_custom_command(POST_BUILD TARGET pager COMMAND ${CMAKE_CURRENT_LIST_DIR}/autotag.sh push)

# Add a build target for each test program
add_executable(app app.cpp)
target_link_libraries(app libapp ${CMAKE_DL_LIBS})
