# The tests: the P3D parser (TOOL-007). Built from the few sources they need,
# not the game, so that they build and run without a window or the game's
# libraries; the game's files are used when they are there.
file(GLOB DONUT_MATH_SOURCES "${CMAKE_SOURCE_DIR}/src/Core/Math/*.cpp")
add_executable(donut_tests
	Main.cpp
	P3DTests.cpp
	TextBibleTests.cpp
	${CMAKE_SOURCE_DIR}/src/Core/File.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Log.cpp
	${CMAKE_SOURCE_DIR}/src/Core/MemoryStream.cpp
	${CMAKE_SOURCE_DIR}/src/P3D/P3DChunk.cpp
	${CMAKE_SOURCE_DIR}/src/P3D/P3DFile.cpp
	${CMAKE_SOURCE_DIR}/src/P3D/P3D.generated.cpp
	${CMAKE_SOURCE_DIR}/src/UI/TextBible.cpp
	${DONUT_MATH_SOURCES})
target_include_directories(donut_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(donut_tests PRIVATE DONUT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(donut_tests PRIVATE fmt::fmt ZLIB::ZLIB)
add_test(NAME donut_tests COMMAND donut_tests)
if (_CXX_FILESYSTEM_HAVE_HEADER)
	target_compile_definitions(donut_tests PRIVATE DONUT_HAS_FILESYSTEM)
elseif (_CXX_FILESYSTEM_HAVE_EXPERIMENTAL_HEADER)
	target_compile_definitions(donut_tests PRIVATE DONUT_HAS_FILESYSTEM_EXPERIMENTAL)
endif()
