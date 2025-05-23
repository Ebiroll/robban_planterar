cmake_minimum_required(VERSION 3.15)
project(RobbanPlanterar)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# Find required packages
find_package(raylib REQUIRED)

# Add executable
add_executable(robban_planterar
    robban.cpp
    NetworkManager.cpp
)

# Link libraries
target_link_libraries(robban_planterar
    raylib
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(robban_planterar winmm)
elseif(APPLE)
    target_link_libraries(robban_planterar "-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL")
elseif(UNIX)
    target_link_libraries(robban_planterar GL m pthread dl rt X11)
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(robban_planterar PRIVATE /W4)
else()
    target_compile_options(robban_planterar PRIVATE -Wall -Wextra -pedantic)
endif()

# Copy resources (if any)
# file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})

# For WebRTC support (optional, requires external libraries)
option(ENABLE_WEBRTC "Enable WebRTC networking" OFF)

if(ENABLE_WEBRTC)
    # Find WebRTC library (you'll need to install libwebrtc)
    # find_package(WebRTC REQUIRED)
    # target_link_libraries(robban_planterar WebRTC::WebRTC)
    target_compile_definitions(robban_planterar PRIVATE ENABLE_WEBRTC)
endif()

# Install targets
install(TARGETS robban_planterar
    RUNTIME DESTINATION bin
)

# Package configuration
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)