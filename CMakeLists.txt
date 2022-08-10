project (edj_reload_album_art LANGUAGES CXX C)
cmake_minimum_required (VERSION 3.3)

# --- some general config

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# --- find the OpenImageIO library on the development system

# get the path for all homebrew files

execute_process (
    COMMAND bash -c "brew --prefix"
    OUTPUT_VARIABLE brewPrefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# --- our source files

set (HEADERS
    sqlite3.h
    sqlite3ext.h
    )

set (SOURCES
    main.cpp
    sqlite3.c
)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES} )

#find_library( OIIO_LIB NAMES OpenImageIO )
#find_library( OIIO_UTIL_LIB NAMES OpenImageIO_Util )

find_package(ImageMagick 7 COMPONENTS Magick++ MagickCore)
if(ImageMagick_FOUND)
    # Find Imagemagick Library directory
    get_filename_component(MAGICK_LIB_DIR ${ImageMagick_MagickCore_LIBRARY} DIRECTORY)

    # we get "../lib/.." but this is too deep. one level iup

    set(MAGICK_LIB_DIR ${MAGICK_LIB_DIR}/.. )
   
    # Find where Magick++-config lives
    file(GLOB_RECURSE MAGICK_CONFIG FOLLOW_SYMLINKS ${MAGICK_LIB_DIR}/Magick++-config)

    # Ask about CXX and lib flags/locations
    set(MAGICK_CONFIG ${MAGICK_CONFIG} CACHE STRING "Path to Magick++-config utility")
    execute_process(COMMAND "${MAGICK_CONFIG}" "--cxxflags" OUTPUT_VARIABLE MAGICK_CXX_FLAGS)
    execute_process(COMMAND "${MAGICK_CONFIG}" "--libs" OUTPUT_VARIABLE MAGICK_LD_FLAGS)
    # Add these to cache
    set(MAGICK_CXX_FLAGS "${MAGICK_CXX_FLAGS}" CACHE STRING "ImageMagick configuration specific compilation flags." )
    set(MAGICK_LD_FLAGS  "${MAGICK_LD_FLAGS}" CACHE STRING "ImageMagick configuration specific linking flags.")
    # Split into list:
    string(REGEX MATCHALL "([^\ ]+)" MAGICK_CXX_FLAGS "${MAGICK_CXX_FLAGS}")
    string(REGEX MATCHALL "([^\ ]+)" MAGICK_LD_FLAGS "${MAGICK_LD_FLAGS}")
    # Remove trailing whitespace (CMAKE warns about this)
    string(STRIP "${MAGICK_CXX_FLAGS}" MAGICK_CXX_FLAGS)
    string(STRIP "${MAGICK_LD_FLAGS}" MAGICK_LD_FLAGS)

    target_compile_options(${PROJECT_NAME} PUBLIC ${MAGICK_CXX_FLAGS})

    ##target_include_directories(${PROJECT_NAME} PUBLIC "/opt/homebrew/Cellar/imagemagick/7.1.0-45/include/ImageMagick-7")

    target_link_options(${PROJECT_NAME} PUBLIC ${MAGICK_LD_FLAGS})

    message(STATUS ${MAGICK_LD_FLAGS})
    message(STATUS ${MAGICK_CXX_FLAGS})
    
  
endif(ImageMagick_FOUND)

# --- add taglib to our build env

add_subdirectory(taglib-1.12)

### there has to be a more easy way to propagete the include dirs from target lib. But this works.

get_property(taglib_includes DIRECTORY taglib-1.12/taglib PROPERTY INCLUDE_DIRECTORIES)

# --- our target definitions

target_link_libraries(${PROJECT_NAME} tag )

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/taglib-1.12 )
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/taglib-1.12/taglib )
target_include_directories(${PROJECT_NAME} PUBLIC ${taglib_includes} )


