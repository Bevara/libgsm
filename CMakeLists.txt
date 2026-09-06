cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBGSM_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_gsm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/gsm_stubs.c
)

SET(LIBGSM_LIBS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libgsm.a
)

SET(LIBGSM_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libgsm
        "${LIBGSM_SRC}"
        "${LIBGSM_LIBS}"
        []
        ""
        "${LIBGSM_INC}"
        ""
        "1")
