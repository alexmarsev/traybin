cmake_minimum_required(VERSION 2.4)

if(NOT WIN32) 
	message(FATAL_ERROR "win32 only")
endif()

if(MSVC)
	if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GL")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /LTCG /OPT:REF")
	endif()
endif()

add_executable(traybin WIN32 traybin.cpp)
