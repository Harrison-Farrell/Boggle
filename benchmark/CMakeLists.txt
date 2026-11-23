AddBenchmarks(bm_BoggleSolver)

target_include_directories(bm_BoggleSolver PRIVATE ${CMAKE_SOURCE_DIR}/src)

find_package(Threads REQUIRED)

target_link_libraries(bm_BoggleSolver PRIVATE 
    ${LIBRARY_PREFIX}::Dictionary 
    ${LIBRARY_PREFIX}::BoggleBoard 
    ${LIBRARY_PREFIX}::BoggleSolver
    Threads::Threads
)

# Copy the dictionaries used by the benchmark to the target's output directory
add_custom_command(TARGET bm_BoggleSolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/resources/basic_english_words_2000.txt"
        "${CMAKE_SOURCE_DIR}/resources/boggle_words_138735.txt"
        "$<TARGET_FILE_DIR:bm_BoggleSolver>"
    COMMENT "Copying dictionaries to target directory"
)
