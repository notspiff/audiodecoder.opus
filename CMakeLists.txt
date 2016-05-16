project(audiodecoder.opus)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(Ogg REQUIRED)
find_package(Opus REQUIRED)
find_package(OpusFile REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${OGG_INCLUDE_DIRS}
                    ${OPUS_INCLUDE_DIRS}
                    ${OPUSFILE_INCLUDE_DIRS})

add_definitions(-D__STDC_FORMAT_MACROS)
set(OPUS_SOURCES src/OpusCodec.cpp)

set(DEPLIBS ${OGG_LIBRARIES}
            ${OPUSFILE_LIBRARIES}
            ${OPUS_LIBRARIES})

build_addon(audiodecoder.opus OPUS DEPLIBS)
