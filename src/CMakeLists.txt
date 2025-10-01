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

# Platform-specific raylib configuration
if(PLATFORM_WEB)
    # Web-specific raylib settings - disable desktop features
    set(SUPPORT_FILEFORMAT_WAV OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_OGG OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_MP3 OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_FLAC OFF CACHE BOOL "" FORCE)
    
    # Disable desktop windowing systems for web
    set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_COCOA OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WIN32 OFF CACHE BOOL "" FORCE)
    
    # Force web platform
    set(PLATFORM "Web" CACHE STRING "" FORCE)
    
    message(STATUS "Configuring raylib for Web platform")
else()
    # Native builds - ensure we have proper windowing system
    if(UNIX AND NOT APPLE)
        # Linux-specific settings
        find_package(PkgConfig REQUIRED)
        
        # Check for required X11 libraries
        find_package(X11 REQUIRED)
        if(NOT X11_FOUND)
            message(FATAL_ERROR "X11 not found. Please install: sudo apt-get install libx11-dev")
        endif()
        
        # Check for XRandR
        pkg_check_modules(XRANDR REQUIRED xrandr)
        if(NOT XRANDR_FOUND)
            message(FATAL_ERROR "XRandR not found. Please install: sudo apt-get install libxrandr-dev")
        endif()
        
        # Check for other required libraries
        pkg_check_modules(XCursor REQUIRED xcursor)
        if(NOT XCursor_FOUND)
            message(FATAL_ERROR "XCursor not found. Please install: sudo apt-get install libxcursor-dev")
        endif()
        
        pkg_check_modules(Xi REQUIRED xi)
        if(NOT Xi_FOUND)
            message(FATAL_ERROR "Xi not found. Please install: sudo apt-get install libxi-dev")
        endif()
        
        pkg_check_modules(Xinerama REQUIRED xinerama)
        if(NOT Xinerama_FOUND)
            message(FATAL_ERROR "Xinerama not found. Please install: sudo apt-get install libxinerama-dev")
        endif()
        
        message(STATUS "All required X11 libraries found")
    endif()
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
    
    # Export runtime methods needed for audio and networking (including heap arrays for Web Audio API)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32','HEAPU8','HEAP16','HEAPU16','HEAP32','HEAPU32','allocateUTF8','UTF8ToString']")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_FUNCTIONS=['_main','_malloc','_free','_OnPeerReady','_OnPlayerJoined','_OnPlayerLeft','_OnNetworkMessage','_OnHostGameClicked','_OnJoinGameClicked','_OnDisconnectClicked','_JS_InitPeerNetwork','_JS_CreateRoom','_JS_JoinRoom','_JS_BroadcastMessage','_JS_GetRoomId','_JS_GetConnectionCount','_JS_DisconnectPeer']")

    # Include JavaScript library for PeerJS networking
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --js-library ${CMAKE_SOURCE_DIR}/peer_network.js")
    
    # Embed robban.png for web builds
    if(EXISTS "${CMAKE_SOURCE_DIR}/robban.png")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/robban.png@robban.png")
        message(STATUS "Embedding robban.png for web build")
    else()
        message(WARNING "robban.png not found - web build will use fallback graphics")
    endif()
    
    # Embed sound files for web builds (sounds are in parent directory)
    if(EXISTS "${CMAKE_SOURCE_DIR}/../souds/shoot.wav")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/../souds/shoot.wav@souds/shoot.wav")
        message(STATUS "Embedding souds/shoot.wav for web build")
    else()
        message(WARNING "souds/shoot.wav not found at ${CMAKE_SOURCE_DIR}/../souds/shoot.wav")
    endif()
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/../souds/axe-cut-1.wav")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/../souds/axe-cut-1.wav@souds/axe-cut-1.wav")
        message(STATUS "Embedding souds/axe-cut-1.wav for web build")
    else()
        message(WARNING "souds/axe-cut-1.wav not found at ${CMAKE_SOURCE_DIR}/../souds/axe-cut-1.wav")
    endif()
    
    # Memory settings
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s INITIAL_MEMORY=67108864")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    
    # File system settings (needed for preloaded files)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s FORCE_FILESYSTEM=1")
    
    # Optimization settings
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --closure 1")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O1")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
    endif()
    
    # Shell file customization - use multiplayer shell
    if(EXISTS "${CMAKE_SOURCE_DIR}/shell_multiplayer.html")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell_multiplayer.html")
        message(STATUS "Using multiplayer HTML shell")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/shell_minimal.html")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell_minimal.html")
        message(STATUS "Using minimal HTML shell")
    endif()
    
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
    
    # Remove the USE_GLFW from compile options - it's a linker flag
    # if(PLATFORM_WEB)
    #     target_compile_options(robban_planterar PRIVATE -s USE_GLFW=3)
    # endif()
endif()

# Copy resources to build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/robban.png")
    configure_file("${CMAKE_SOURCE_DIR}/robban.png" "${CMAKE_BINARY_DIR}/robban.png" COPYONLY)
    message(STATUS "Copying robban.png to build directory")
else()
    message(WARNING "robban.png not found - game will use fallback graphics")
endif()

# Copy sound files to build directory (sounds are in parent directory)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/souds")
if(EXISTS "${CMAKE_SOURCE_DIR}/../souds/shoot.wav")
    configure_file("${CMAKE_SOURCE_DIR}/../souds/shoot.wav" "${CMAKE_BINARY_DIR}/souds/shoot.wav" COPYONLY)
    message(STATUS "Copying souds/shoot.wav to build directory")
else()
    message(WARNING "souds/shoot.wav not found for copying")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/../souds/axe-cut-1.wav")
    configure_file("${CMAKE_SOURCE_DIR}/../souds/axe-cut-1.wav" "${CMAKE_BINARY_DIR}/souds/axe-cut-1.wav" COPYONLY)
    message(STATUS "Copying souds/axe-cut-1.wav to build directory")
else()
    message(WARNING "souds/axe-cut-1.wav not found for copying")
endif()

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

# Add option for unit test mode
option(UNIT_TEST "Build sprite unit test instead of game" OFF)

if(UNIT_TEST)
    target_compile_definitions(robban_planterar PRIVATE UNIT_TEST)
    message(STATUS "Building in UNIT_TEST mode - sprite debugging")
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