cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

SET(BUILD_SHARED_LIBS FALSE CACHE BOOL "" FORCE)
add_subdirectory(asyncplusplus)
add_subdirectory(backward-cpp)
add_subdirectory(enet)
include(glm_interface.cmake)
include(gsl_interface.cmake)
include(moodycamel_interface.cmake)
include(nuklear_interface.cmake)

include(magic_get_interface.cmake)

SET(PHYSFS_ARCHIVE_7Z FALSE CACHE BOOL "" FORCE)
SET(PHYSFS_BUILD_SHARED FALSE CACHE BOOL "" FORCE)
add_subdirectory(physfs)


include(plog_interface.cmake)
include(robin-map_interface.cmake)
include(doctest_interface.cmake)

add_subdirectory(sf2)


if(MIRRAGE_BUILD_MESH_CONVERTER)
	find_package(assimp 4.1)
	if(NOT assimp_FOUND)
		add_subdirectory(assimp)
		set_property(TARGET assimp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include" "${CMAKE_CURRENT_BINARY_DIR}/assimp/include")
	endif()

	add_subdirectory(stb_image)
endif()
