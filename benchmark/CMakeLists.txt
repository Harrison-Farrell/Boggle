AddBenchmarks(bm_BoggleSolver)

target_include_directories(bm_BoggleSolver PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(bm_BoggleSolver PRIVATE 
    ${LIBRARY_PREFIX}::Dictionary 
    ${LIBRARY_PREFIX}::BoggleBoard 
    ${LIBRARY_PREFIX}::BoggleSolver
)

# Copy a file to the target's output directory after build
add_custom_command(TARGET bm_BoggleSolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/resources/basic_english_words_2000.txt"
        "$<TARGET_FILE_DIR:bm_BoggleSolver>/basic_english_words_2000.txt"
    COMMENT "Copying file to target directory"
)
