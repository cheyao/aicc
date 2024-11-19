cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(
	aicc
	LANGUAGES C
	VERSION 1.0
	DESCRIPTION "C compiler, but compiled and optimized by AI ✨"
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed! Create a build directory and run CMake from there.")
endif()

set(SRC
src/main.c
)

add_executable(aicc ${SRC})

if(CMAKE_BUILD_TYPE STREQUAL Debug)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	target_compile_options(aicc PRIVATE -Wall -Wextra -Werror -g -O0 -Wundef -Wshadow -Wpointer-arith -Wpointer-arith -Wwrite-strings -Wno-unknown-warning-option -fdiagnostics-color=always)
	target_compile_definitions(aicc PRIVATE -DDEBUG -D_DEBUG)
else()
	target_compile_options(aicc PRIVATE -O3)

	# Enable LTO for non debug
	include(CheckIPOSupported)
	check_ipo_supported(RESULT LTOSupported OUTPUT error)

	# Web takes too long to compile with LTO
	if(LTOSupported)
		message(STATUS "IPO / LTO enabled")

		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
		set_property(TARGET aicc PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	elseif(NOT DEBUG)
		message(STATUS "IPO / LTO not supported: <${error}>")
	endif()
endif()
