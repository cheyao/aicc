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

option(OPENAI_API "URL to the API for OpenAI" "https://api.openai.com/v1/chat/completions")

if(NOT DEFINED OPENAI_API_KEY)
	message(FATAL_ERROR "You must supply your own API KEY with -DOPENAI_API_KEY=<key>")
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed! Create a build directory and run CMake from there.")
endif()

set(SRC
src/main.c
)

add_executable(aicc ${SRC})

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if(CMAKE_BUILD_TYPE STREQUAL Debug)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	target_compile_options(aicc PRIVATE -Wall -Wextra -Werror -g -O0 -Wundef -Wshadow -Wpointer-arith -Wpointer-arith -Wwrite-strings -Wno-unknown-warning-option -fdiagnostics-color=always)
	target_compile_definitions(aicc PRIVATE -DDEBUG -D_DEBUG -DOPENAI_API=${OPENAI_API} -DOPENAI_API_KEY=${OPENAI_API_KEY})
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

target_include_directories(aicc PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(aicc PRIVATE ${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
target_link_libraries(aicc LLVM)

find_package(CURL REQUIRED) 
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(aicc ${CURL_LIBRARIES})

