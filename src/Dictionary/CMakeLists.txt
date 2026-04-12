set(LIBRARY_NAME "Dictionary")

add_library(${LIBRARY_NAME} STATIC)
add_library(${LIBRARY_PREFIX}::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

target_sources(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Dictionary.cpp
)

target_link_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)