cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_gui LANGUAGES CXX)

# need to be specified with the sources for correct syntax highlighting in some IDEs
file(GLOB_RECURSE HEADER_FILES
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/*.h*
)

add_library(mirrage_gui STATIC
	src/color_picker.cpp
	src/gui.cpp
	src/menu.cpp
	${HEADER_FILES}
)
add_library(mirrage::gui ALIAS mirrage_gui)

target_compile_options(mirrage_gui PRIVATE -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-braces)


target_include_directories(mirrage_gui PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(mirrage_gui
	PUBLIC
		mirrage::utils
		gsl
		glm
		nuklear
		mirrage::asset
		mirrage::input
		sf2
)
   

install(TARGETS mirrage_gui EXPORT mirrage_gui_targets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
install(
	DIRECTORY ${CMAKE_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
install(EXPORT mirrage_gui_targets
	FILE mirrage_gui.cmake
	NAMESPACE mirrage::
	DESTINATION lib/cmake/mirrage
)
