cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(mirrage_mesh_converter LANGUAGES CXX)


add_executable(mesh_converter
	animation_parser.cpp
	animation_parser.hpp
	common.hpp
	filesystem.cpp
	filesystem.hpp
	main.cpp
	material_parser.cpp
	material_parser.hpp
	model_parser.cpp
	model_parser.hpp
	skeleton_parser.cpp
	skeleton_parser.hpp
	${BACKWARD_ENABLE}
)

add_backward(mesh_converter)

target_link_libraries(mesh_converter
	PRIVATE
		mirrage::mirrage
		mirrage::renderer
		mirrage::utils
		assimp
		sf2
		stb_image
		doctest
)

