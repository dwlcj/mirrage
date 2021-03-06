cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_demo LANGUAGES CXX)

file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_executable(demo ${HEADER_FILES}
	src/systems/nim_system.cpp
	src/game_engine.cpp
	src/main.cpp
	src/meta_system.cpp
	src/test_screen.cpp
	${BACKWARD_ENABLE}
)
add_backward(demo)

set(MIRRAGE_DEFAULT_COMPILER_ARGS ${MIRRAGE_DEFAULT_COMPILER_ARGS})
target_compile_options(demo PRIVATE ${MIRRAGE_DEFAULT_COMPILER_ARGS})

target_link_libraries(demo
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		plog
		glm::glm
		sf2
		doctest
)

option(MIRRAGE_COMPILE_SHADERS "Compile demo shaders" OFF)
if(MIRRAGE_COMPILE_SHADERS)
	include(shaders.cmake)
	add_dependencies(demo demo_shaders)
endif()

option(MIRRAGE_EXPORT_EXECUTABLE "Export executable" OFF)
if(MIRRAGE_EXPORT_EXECUTABLE)
	install(DIRECTORY ${ROOT_DIR}/assets/ DESTINATION bin)
	install(TARGETS demo RUNTIME DESTINATION bin)
endif()

if(COMMAND cotire)
	cotire(demo)
endif()
