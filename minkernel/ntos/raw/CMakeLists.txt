cmake_minimum_required(VERSION 3.29)

set(TARGET raw)

set(${TARGET}_HEADERS nodetype.h
                rawprocs.h
                rawstruc.h)
set(${TARGET}_SOURCES cleanup.c
                close.c
                create.c
                fileinfo.c
                fsctrl.c
                strucsup.c
                rawdisp.c
                rawinit.c
                readwrit.c
                volinfo.c)

add_library(${TARGET} STATIC ${${TARGET}_HEADERS} ${${TARGET}_SOURCES})
target_compile_definitions(${TARGET} PRIVATE ${NT_DEFS})
target_compile_options(${TARGET} PRIVATE ${NT_C_FLAGS})
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${NT_INCDIRS} ${SDKTOOLS_INC_PATH})
set_property(TARGET ${TARGET} PROPERTY FOLDER "Kernel")
