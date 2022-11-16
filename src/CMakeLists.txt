set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/csl.cc)

add_library(csl SHARED
    csl.h
    ${SRC_FILES})