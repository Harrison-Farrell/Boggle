include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
	GIT_SHALLOW 1
)

# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

option(INSTALL_GMOCK OFF)
option(INSTALL_GTEST OFF)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

macro(AddTests target)
	add_executable(${target} ${target}.cpp)

	target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
	
	target_link_libraries(${target} PRIVATE
    gtest
    gtest_main
	${LIBRARY_PREFIX}::Dictionary
	${LIBRARY_PREFIX}::BoggleBoard
	${LIBRARY_PREFIX}::BoggleSolver
	)

	set_target_properties(${target} PROPERTIES
        DEBUG_POSTFIX "_Debug"
        RELEASE_POSTFIX "_Release"
        RELWITHDEBINFO_POSTFIX "_RelWithDebInfo"
        MINSIZEREL_POSTFIX "_MinSizeRel"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test"
	)
	gtest_discover_tests(${target})
endmacro()
