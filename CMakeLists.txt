cmake_minimum_required(VERSION 2.8)

project(siftsearch)

include(ExternalProject)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -pedantic -Wextra -O2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_definitions(-D_BSD_SOURCE)

# OpenMP
find_package(OpenMP REQUIRED)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

# OpenCV
find_package(OpenCV REQUIRED)

# GTK+ 2.0
find_package(GTK2 REQUIRED)

# OpenSIFT
ExternalProject_Add(
  opensift
  PREFIX ${CMAKE_BINARY_DIR}/opensift
  GIT_REPOSITORY https://github.com/robwhess/opensift.git
  TIMEOUT 10
  UPDATE_COMMAND git pull
  CONFIGURE_COMMAND ""
  BUILD_COMMAND $(MAKE) libopensift.a
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE 1
  LOG_DOWNLOAD ON
)

ExternalProject_Get_Property(opensift source_dir)
set(OPENSIFT_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL "Path to include folder for OpenSIFT")
set(OPENSIFT_LIBRARIES -lopensift)
link_directories(${source_dir}/lib/)
include_directories(${OPENSIFT_INCLUDE_DIR})

add_executable(
    siftsearch
    src/main.c
    src/util.c
    src/tree.c
)

target_link_libraries(siftsearch ${OPENSIFT_LIBRARIES} ${OpenCV_LIBS} ${GTK2_LIBRARIES} -lgdbm) 

install (
	TARGETS siftsearch
        RUNTIME DESTINATION bin
	)

