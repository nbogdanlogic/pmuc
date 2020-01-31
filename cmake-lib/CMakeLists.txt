project(RVM)
cmake_minimum_required(VERSION 2.8)

# Set C++11 flags to makesome function work
if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

# Add OpenGL dependencies
set (OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

add_library(${PROJECT_NAME} SHARED	../src/api/rvmcolorhelper.cpp
									../src/api/rvmmeshhelper.cpp
									../src/api/rvmparser.cpp
									../src/api/rvmreader.cpp
									../src/api/vector3f.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "../src/api/rvm.h;../src/api/lib_export.h;../src/api/rvmcolorhelper.h;../src/api/rvmmeshhelper.h;../src/api/rvmparser.h;../src/api/rvmprimitive.h;../src/api/rvmreader.h;../src/api/vector3f.h"
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)
# install(TARGETS ${PROJECT_NAME} PUBLIC_HEADER DESTINATION include)

# Include OpenGL lib
target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# 
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )