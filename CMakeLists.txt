project(engine)

cmake_minimum_required(VERSION 3.1)

# Include local settings.
include(cmake_user.txt)
message(STATUS "GAME_PATH=${GAME_PATH}")
message(STATUS "ASSET_PATH=${ASSET_PATH}")

# Import compiler flags defined in another file.
include(build/cmake/engine-compiler-flags.cmake)

# QUIRK: Surpress warning with cmake 3.0 on OSX.
set(CMAKE_MACOSX_RPATH 1)

# Compile options
option(ENABLE_CONSOLE "Compile with console support" ON)
option(ENABLE_SHARED "Enable game hotswapping" ON)

# QUIRK: Define M_PI on Windows.
add_definitions(-D_USE_MATH_DEFINES)

# Enable asset hotswap.
add_definitions(-DVFS_ENABLE_FILEWATCH)

# Check for compatibility.
include(CheckFunctionExists)
check_function_exists(strnlen HAVE_STRNLEN)
if(HAVE_STRNLEN)
    add_definitions(-DHAVE_STRNLEN)
endif()

set(ENGINE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(ENGINE_INCLUDES ${ENGINE_PATH})

# Store DLLs alongside engine.exe.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CACHE STRING "Static Library output directory" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CACHE STRING "Shared Library output directory" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CACHE STRING "Executable output directory" FORCE)

if(NOT EMSCRIPTEN)
    # Library: GLFW.
    set(BUILD_SHARED_LIBS ${ENABLE_SHARED} CACHE BOOL "Build shared libraries" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
    add_subdirectory(lib/glfw)
    list(APPEND ENGINE_INCLUDES lib/glfw/include)
    list(APPEND ENGINE_EXTRA_LIBS glfw ${GLFW_LIBRARIES})

    # Library: glew.
    if(ENABLE_SHARED)
        list(APPEND ENGINE_EXTRA_LIBS glew ${GLEW_LIBRARIES})
    else()
        list(APPEND ENGINE_EXTRA_LIBS glew_s ${GLEW_LIBRARIES})
    endif()
    add_subdirectory(lib/glew/build/cmake)
    list(APPEND ENGINE_INCLUDES lib/glew/include)

    # Library: cJSON.
    add_subdirectory(lib/cjson)
    if(ENABLE_SHARED)
        list(APPEND ENGINE_EXTRA_LIBS cJSON.shared)
    else()
        list(APPEND ENGINE_EXTRA_LIBS cJSON)
    endif()

    # Library: OpenAL.
    if(NOT APPLE)
        # Build OpenAL-Soft.
        if(ENABLE_SHARED)
            set(LIBTYPE "SHARED" CACHE STRING "Build static library" FORCE)
        else()
            set(LIBTYPE "STATIC" CACHE STRING "Build static library" FORCE)
        endif()
        set(ALSOFT_UTILS OFF CACHE BOOL "Build and install utility programs" FORCE)
        set(ALSOFT_NO_CONFIG_UTIL OFF CACHE BOOL "Disable building the alsoft-config utility" FORCE)
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "Build and install example programs" FORCE)
        set(ALSOFT_CONFIG OFF CACHE BOOL "Install alsoft.conf sample configuration file" FORCE)
        set(ALSOFT_HRTF_DEFS OFF CACHE BOOL "Install HRTF definition files" FORCE)
        set(ALSOFT_INSTALL OFF CACHE BOOL "Install headers and libraries" FORCE)
        # .lib-generation requires GNU 'sed' but MinGW usually ships with old version.
        set(ALSOFT_BUILD_IMPORT_LIB OFF CACHE BOOL "Build an import .lib using dlltool (requires sed)" FORCE)
        add_subdirectory(lib/openal)
        list(APPEND ENGINE_INCLUDES lib/openal/include/AL)

        # OpenAL has different names on different platforms.
        if(WIN32)
            list(APPEND ENGINE_EXTRA_LIBS OpenAL32)
        else()
            list(APPEND ENGINE_EXTRA_LIBS openal)
        endif()
    else()
        # Use the OS X OpenAL framework.
        find_library(OPENAL_LIBRARY OpenAL)
        find_path(OPENAL_INCLUDE_DIR al.h)
        list(APPEND ENGINE_INCLUDES ${OPENAL_INCLUDE_DIR})
        list(APPEND ENGINE_EXTRA_LIBS ${OPENAL_LIBRARY})
    endif()
else()
    # glfw and glew is provided by emscripten
    option(USE_GLFW_3 "Use glfw3" ON)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file test_assets/test.png")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file test_assets/vivaldi.ogg")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file test_assets/paddle.png")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s TOTAL_MEMORY=100000000")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-warn-absolute-paths")
endif()

# Include stb.h
list(APPEND ENGINE_INCLUDES lib/)

set(ENGINE_SOURCES_LOCAL math4.c core_console.c graphics.c shader.c input.c texture.c
        color.c sound.c vfs.c atlas.c monotext.c str.c list.c console.c
        spritebatch.c animatedsprites.c alist.c core.c core_argv.c core_reload.c
        particles.c collide.c drawable.c)
set(ENGINE_HEADERS_LOCAL math4.h core_console.h graphics.h shader.h input.h texture.h
        color.h sound.h vfs.h atlas.h monotext.h str.h list.h console.h
        spritebatch.h animatedsprites.h alist.h core.h core_argv.h core_reload.h
        particles.h game.h collide.h geometry.h drawable.h)

# Top-down sources
set(ENGINE_SOURCES_TOP_DOWN top-down/tiles.c)
set(ENGINE_HEADERS_TOP_DOWN top-down/tiles.h)

list(APPEND ENGINE_SOURCES_LOCAL ${ENGINE_SOURCES_TOP_DOWN})
list(APPEND ENGINE_HEADERS_LOCAL ${ENGINE_HEADERS_TOP_DOWN})

# Prepend all sources with absolute engine path (because of reasons...)
set(ENGINE_SOURCES "")
foreach(src ${ENGINE_SOURCES_LOCAL})
    list(APPEND ENGINE_SOURCES ${ENGINE_PATH}/${src})
endforeach()
set(ENGINE_HEADERS "")
foreach(src ${ENGINE_HEADERS_LOCAL})
    list(APPEND ENGINE_HEADERS ${ENGINE_PATH}/${src})
endforeach()

# Include all source directories
include_directories(${ENGINE_INCLUDES})

# Asset generator executable
add_executable(generate_assets ${ENGINE_PATH}/generate_assets.c ${ENGINE_PATH}/vfs.c ${ENGINE_PATH}/vfs.h ${ENGINE_PATH}/alist.c ${ENGINE_PATH}/alist.h ${ENGINE_PATH}/sound.h)
set_property(TARGET generate_assets PROPERTY C_STANDARD 99)
set_property(TARGET generate_assets APPEND_STRING PROPERTY INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR})
target_link_libraries(generate_assets glfw ${ENGINE_EXTRA_LIBS})

# Where to store generated assets files.
set(ASSETS_STAMP_FILE "${CMAKE_BINARY_DIR}/assets.stamp")
set(ASSETS_C ${CMAKE_BINARY_DIR}/assets.c)
set(ASSETS_H ${CMAKE_BINARY_DIR}/assets.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Compare assets directory, maybe regenerate assets.c
add_custom_target(assets_check_update ALL
    COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${ASSET_PATH} -DSTAMP_FILE=${ASSETS_STAMP_FILE} -P ${ENGINE_PATH}/build/cmake/dir_checksum.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Calculating assets checksum...")

# Mark asset files as "generated" so cmake doesn't complain.
set_source_files_properties(${ASSETS_C} ${ASSETS_H} PROPERTIES GENERATED TRUE)

# Creates assets.{c,h} if ASSETS_STAMP_FILE was changed.
add_custom_command(OUTPUT ${ASSETS_C} ${ASSETS_H}
    COMMAND generate_assets ${ASSET_PATH}
    DEPENDS generate_assets
    DEPENDS assets_check_update
    DEPENDS ${ASSETS_STAMP_FILE}
    COMMENT "Generating assets.c")

# Targets can depend on this target to generate assets.{c,h}.
add_custom_target(generate_assets_do DEPENDS ${ASSETS_C} ${ASSETS_H})

# Add a game library.
function(add_game_library game_name)
    # Mark asset files as "generated".
    set_source_files_properties(${ASSETS_C} ${ASSETS_H} PROPERTIES GENERATED TRUE)

    # Create library.
    add_library(${game_name}_obj OBJECT ${ARGN} ${ASSETS_C} ${ENGINE_SOURCES} ${ENGINE_HEADERS})

    # Store as dependencies that will be applied to the 'engine' target later.
    if(ENABLE_SHARED)
        add_library(${game_name} SHARED $<TARGET_OBJECTS:${game_name}_obj>)
        set(ENGINE_DEPS ${game_name}_obj ${game_name} CACHE INTERNAL "Engine dependencies" FORCE)

        # Need HACK_POST_BUILD for Visual Studio 2013.
        if(MSVC12)
            set(HACK_POST_BUILD 1)
        endif()

        # Output shared library into asset path.
        if(HACK_POST_BUILD)
            message("With HACK_POST_BUILD")
            # HACK: On some platforms, this method works best for hotswapping the
            # shared library.
            set_target_properties(${game_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${ASSET_PATH}"
                LIBRARY_OUTPUT_DIRECTORY "${ASSET_PATH}")
        else()
            add_custom_command(TARGET ${game_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${game_name}>" ${ASSET_PATH}
                COMMENT "Copying ${game_name} shared library to ASSET_PATH")
        endif()
    else()
        add_library(${game_name}_static STATIC $<TARGET_OBJECTS:${game_name}_obj>)
        set(ENGINE_DEPS ${game_name}_obj ${game_name}_static CACHE INTERNAL "Engine dependencies" FORCE)
    endif()

    # Generate assets.{c,h} if necessary.
    add_dependencies(${game_name}_obj generate_assets_do)

    # Compile with console output support?
    if(ENABLE_CONSOLE)
        target_compile_definitions(${game_name}_obj PRIVATE CONSOLE_ENABLE)
    endif()

    # Compile as shared library?
    if(ENABLE_SHARED)
        target_compile_definitions(${game_name}_obj PRIVATE ENABLE_SHARED)
    endif()

    # Compile with DEBUG flag?
    target_compile_definitions(${game_name}_obj PRIVATE $<$<CONFIG:Debug>:DEBUG>)

    # Compile with C99.
    set_property(TARGET ${game_name}_obj PROPERTY C_STANDARD 99)

    # Compile with correct flags.
    set_property(TARGET ${game_name}_obj PROPERTY COMPILE_FLAGS "${ENGINE_COMPILER_FLAGS}")
    set_property(TARGET ${game_name}_obj PROPERTY LINK_FLAGS "${ENGINE_LINKER_FLAGS}")

    # Link libraries.
    if(ENABLE_SHARED)
        target_link_libraries(${game_name} ${ENGINE_EXTRA_LIBS})
        set(ENGINE_GAME_LIBRARY "" CACHE INTERNAL "Do not link against game library" FORCE)

        # Convenience target to compile and run the engine executable with the same
        # asset path as in cmake_user.
        add_custom_target(run_${game_name}
            COMMAND engine --windowed --mount "${ASSET_PATH}" --game "$<TARGET_FILE_NAME:${game_name}>"
            DEPENDS ${game_name} engine)
    else()
        target_link_libraries(${game_name}_static ${ENGINE_EXTRA_LIBS})
        set(ENGINE_GAME_LIBRARY "${game_name}_static" CACHE INTERNAL "Link against game library" FORCE)

        # Convenience target to compile and run the engine executable with the same
        # asset path as in cmake_user.
        add_custom_target(run_${game_name}
            COMMAND engine --windowed --mount "${ASSET_PATH}"
            DEPENDS ${game_name}_static engine)
    endif()
endfunction()

# Build game library.
add_subdirectory(${GAME_PATH})

# Main executable.
include_directories(${GAME_PATH})
add_executable(engine main.c ${ASSETS_C} ${ASSETS_H} ${ENGINE_SOURCES} ${ENGINE_HEADERS})
#add_executable(engine main.c ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Engine depends on game library.
add_dependencies(engine ${ENGINE_DEPS})

# Add DEBUG define when building debug executables.
if(ENABLE_SHARED)
    target_compile_definitions(engine PRIVATE LOAD_SHARED)
endif()
target_compile_definitions(engine PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_definitions(generate_assets PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# Console capture stdout support
if(ENABLE_CONSOLE)
    target_compile_definitions(engine PRIVATE CONSOLE_ENABLE)
endif()

# GCC compiler settings.
if(CMAKE_COMPILER_IS_GNUCC)
    # DEBUG: Lightweight buffer overflow detection.
    target_compile_options(engine PRIVATE $<$<CONFIG:Debug>:-O2>)
    target_compile_definitions(engine PRIVATE $<$<CONFIG:Debug>:_FORTIFY_SOURCE=2>)
    target_compile_definitions(generate_assets PRIVATE $<$<CONFIG:Debug>:_FORTIFY_SOURCE=2>)

    # QUIRK: stb.h requires all these warnings to be silenced, but it seems
    # warnings have to be set for the source file including the header...
    set(STB_HEADER vfs.c)
    set_property(SOURCE ${STB_HEADER} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-maybe-uninitialized ")
    set_property(SOURCE ${STB_HEADER} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-strict-aliasing ")
    set_property(SOURCE ${STB_HEADER} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-unused-but-set-variable ")
    set_property(SOURCE ${STB_HEADER} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-unused-value ")
    set_property(SOURCE ${STB_HEADER} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-unused-function ")
endif()

# Set compiler flags.
set_property(TARGET engine generate_assets PROPERTY COMPILE_FLAGS "${ENGINE_COMPILER_FLAGS}")
set_property(TARGET engine generate_assets PROPERTY LINK_FLAGS "${ENGINE_LINKER_FLAGS}")

# Link.
if(NOT EMSCRIPTEN)
    # Build the engine using the C99 standard.
    set_property(TARGET engine PROPERTY C_STANDARD 99)
    target_link_libraries(engine ${ENGINE_EXTRA_LIBS} ${ENGINE_GAME_LIBRARY})
else()
    target_link_libraries(engine)
endif()
