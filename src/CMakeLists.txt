
add_subdirectory(Dictionary)
add_subdirectory(BoggleBoard)
add_subdirectory(BoggleSolver)

set(APPLICATION_NAME "SolveBoggleBoard")
add_executable(${APPLICATION_NAME} main.cpp)
set_target_properties(${APPLICATION_NAME} PROPERTIES
    DEBUG_POSTFIX "_Debug"
    RELEASE_POSTFIX "_Release"
    RELWITHDEBINFO_POSTFIX "_RelWithDebInfo"
    MINSIZEREL_POSTFIX "_MinSizeRel"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/app"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/app"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/app"
)

target_include_directories(${APPLICATION_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${APPLICATION_NAME} PUBLIC 
    ${LIBRARY_PREFIX}::Dictionary
    ${LIBRARY_PREFIX}::BoggleBoard
    ${LIBRARY_PREFIX}::BoggleSolver
)

# Install rules
install(TARGETS ${APPLICATION_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/
    DESTINATION resources
)