cmake_minimum_required(VERSION 3.15)
project(RobbanPlanterar)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Detect Emscripten
if(EMSCRIPTEN)
    set(PLATFORM_WEB TRUE)
    message(STATUS "Building for Web with Emscripten")
else()
    set(PLATFORM_WEB FALSE)
    message(STATUS "Building for Native platform")
endif()

# Fetch raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)

# Configure raylib options
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

# For web builds, disable some raylib features
if(PLATFORM_WEB)
    set(SUPPORT_FILEFORMAT_WAV OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_OGG OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_MP3 OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_FLAC OFF CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

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
if(PLATFORM_WEB)
    # Emscripten-specific settings
    set_target_properties(robban_planterar PROPERTIES
        SUFFIX ".html"
    )
    
    # Emscripten compiler flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASSERTIONS=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s WASM=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASYNCIFY")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS=1")
    
    # Memory settings
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s INITIAL_MEMORY=67108864") # 64MB
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    
    # WebRTC/Networking settings for web
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s FETCH=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s WEBSOCKET_URL=wss://")
    
    # File system settings
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s FORCE_FILESYSTEM=1")
    
    # Optimization settings
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --closure 1")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O1")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
    endif()
    
    # Shell file customization
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell_minimal.html")
    
elseif(WIN32)
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
    
    # Additional flags for Emscripten
    if(PLATFORM_WEB)
        target_compile_options(robban_planterar PRIVATE -s USE_GLFW=3)
    endif()
endif()

# Copy resources (if any)
# file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})

# For WebRTC support (disabled for web builds due to complexity)
option(ENABLE_WEBRTC "Enable WebRTC networking" OFF)

if(ENABLE_WEBRTC AND NOT PLATFORM_WEB)
    # Find WebRTC library (you'll need to install libwebrtc)
    # find_package(WebRTC REQUIRED)
    # target_link_libraries(robban_planterar WebRTC::WebRTC)
    target_compile_definitions(robban_planterar PRIVATE ENABLE_WEBRTC)
    message(STATUS "WebRTC networking enabled")
elseif(ENABLE_WEBRTC AND PLATFORM_WEB)
    message(WARNING "WebRTC not supported in web builds - using WebSocket fallback")
    target_compile_definitions(robban_planterar PRIVATE ENABLE_WEBSOCKET)
endif()

# Web-specific definitions
if(PLATFORM_WEB)
    target_compile_definitions(robban_planterar PRIVATE PLATFORM_WEB)
endif()

# Install targets
if(NOT PLATFORM_WEB)
    install(TARGETS robban_planterar
        RUNTIME DESTINATION bin
    )
else()
    # For web builds, we want to install the HTML, JS, and WASM files
    install(FILES 
        ${CMAKE_BINARY_DIR}/robban_planterar.html
        ${CMAKE_BINARY_DIR}/robban_planterar.js  
        ${CMAKE_BINARY_DIR}/robban_planterar.wasm
        DESTINATION web
        OPTIONAL
    )
endif()

# Package configuration
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Custom targets for web development
if(PLATFORM_WEB)
    # Create a simple HTTP server target for testing
    add_custom_target(serve
        COMMAND python3 -m http.server 8000
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Starting HTTP server on http://localhost:8000"
    )
    
    # Create target to open browser
    add_custom_target(open
        COMMAND ${CMAKE_COMMAND} -E echo "Opening http://localhost:8000/robban_planterar.html"
        COMMAND python3 -c "import webbrowser; webbrowser.open('http://localhost:8000/robban_planterar.html')"
        DEPENDS robban_planterar
        COMMENT "Opening game in browser"
    )
endif()