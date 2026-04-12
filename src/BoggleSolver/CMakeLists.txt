set(LIBRARY_NAME "BoggleSolver")

add_library(${LIBRARY_NAME} STATIC)
add_library(${LIBRARY_PREFIX}::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

target_sources(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/BoggleSolver.cpp
)

target_link_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${LIBRARY_NAME} PUBLIC
	${LIBRARY_PREFIX}::Dictionary
	${LIBRARY_PREFIX}::BoggleBoard
)