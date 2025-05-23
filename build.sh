#!/bin/bash

# Robban Planterar Build Script
# Supports both native and web (Emscripten) builds

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}=================================${NC}"
    echo -e "${BLUE}    Robban Planterar Builder    ${NC}"
    echo -e "${BLUE}=================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  native          Build for native platform (default)"
    echo "  web             Build for web using Emscripten"
    echo "  clean           Clean build directory"
    echo "  serve           Start web server (web builds only)"
    echo "  open            Build and open in browser (web builds only)"
    echo "  release         Build in release mode"
    echo "  debug           Build in debug mode (default)"
    echo "  help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 native          # Build native version"
    echo "  $0 web release     # Build web version in release mode"
    echo "  $0 web open        # Build web version and open in browser"
    echo "  $0 clean           # Clean build files"
}

check_emscripten() {
    if ! command -v emcc &> /dev/null; then
        print_error "Emscripten not found. Please install and activate Emscripten SDK:"
        echo "  git clone https://github.com/emscripten-core/emsdk.git"
        echo "  cd emsdk"
        echo "  ./emsdk install latest"
        echo "  ./emsdk activate latest"
        echo "  source ./emsdk_env.sh"
        exit 1
    fi
    print_success "Emscripten found: $(emcc --version | head -n1)"
}

check_dependencies() {
    local missing_deps=()
    
    # Check for required system libraries on Linux
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Check for X11 libraries
        if ! pkg-config --exists x11; then
            missing_deps+=("libx11-dev")
        fi
        if ! pkg-config --exists xrandr; then
            missing_deps+=("libxrandr-dev")
        fi
        if ! pkg-config --exists xcursor; then
            missing_deps+=("libxcursor-dev")
        fi
        if ! pkg-config --exists xi; then
            missing_deps+=("libxi-dev")
        fi
        if ! pkg-config --exists xinerama; then
            missing_deps+=("libxinerama-dev")
        fi
        
        if [ ${#missing_deps[@]} -gt 0 ]; then
            print_error "Missing system dependencies:"
            for dep in "${missing_deps[@]}"; do
                echo "  - $dep"
            done
            echo ""
            print_info "Install missing dependencies with:"
            echo "  ./install-deps.sh --system"
            echo ""
            print_info "Or manually on Ubuntu/Debian:"
            echo "  sudo apt-get install ${missing_deps[*]}"
            return 1
        fi
    fi
    
    return 0
}

check_cmake() {
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake 3.15 or later."
        print_info "Install with: ./install-deps.sh --system"
        exit 1
    fi
    local cmake_version=$(cmake --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+')
    print_success "CMake found: $cmake_version"
}

check_python() {
    if ! command -v python3 &> /dev/null; then
        print_warning "Python3 not found. Web server functionality will be limited."
        return 1
    fi
    print_success "Python3 found for web server"
    return 0
}

build_native() {
    local build_type=${1:-Debug}
    
    print_info "Building native version (${build_type})..."
    
    check_cmake
    
    # Check system dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Create build directory
    mkdir -p build_native
    cd build_native
    
    # Configure
    print_info "Configuring with CMake..."
    if ! cmake -DCMAKE_BUILD_TYPE=${build_type} ..; then
        print_error "CMake configuration failed"
        print_info "Common solutions:"
        echo "  - Install missing dependencies: ./install-deps.sh --system"
        echo "  - Update CMake to version 3.15 or later"
        cd ..
        exit 1
    fi
    
    # Build
    print_info "Building..."
    local cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    if ! cmake --build . --config ${build_type} -j${cores}; then
        print_error "Build failed"
        cd ..
        exit 1
    fi
    
    cd ..
    print_success "Native build complete!"
    print_info "Executable: build_native/robban_planterar"
}

build_web() {
    local build_type=${1:-Debug}
    
    print_info "Building web version (${build_type})..."
    
    check_cmake
    check_emscripten
    
    # Check if CMakeLists.txt exists
    if [ ! -f "CMakeLists.txt" ]; then
        print_error "CMakeLists.txt not found in current directory"
        print_info "Make sure you have all the required files"
        exit 1
    fi
    
    # Create build directory
    mkdir -p build_web
    cd build_web
    
    # Configure with Emscripten
    print_info "Configuring with Emscripten..."
    if ! emcmake cmake -DCMAKE_BUILD_TYPE=${build_type} ..; then
        print_error "Emscripten CMake configuration failed"
        print_warning "This might be due to raylib X11 dependency issues in web builds"
        print_info "Trying alternative configuration..."
        
        cd ..
        
        # Try alternative CMakeLists if X11 issues occur
        if [ -f "CMakeLists-web.txt" ]; then
            print_info "Using alternative web configuration..."
            cd build_web
            if ! emcmake cmake -DCMAKE_BUILD_TYPE=${build_type} -f ../CMakeLists-web.txt ..; then
                print_error "Alternative configuration also failed"
                cd ..
                exit 1
            fi
        else
            print_info "Possible solutions:"
            echo "  1. Clean build directory: rm -rf build_web"
            echo "  2. Make sure Emscripten is properly activated: source emsdk/emsdk_env.sh"
            echo "  3. Try updating raylib version in CMakeLists.txt"
            echo "  4. Use the alternative CMakeLists-web.txt if available"
            exit 1
        fi
    fi
    
    # Build
    print_info "Building..."
    if ! emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4); then
        print_error "Emscripten build failed"
        cd ..
        exit 1
    fi
    
    cd ..
    print_success "Web build complete!"
    print_info "Files: build_web/robban_planterar.html, .js, .wasm"
}

serve_web() {
    if [ ! -f "build_web/robban_planterar.html" ]; then
        print_error "Web build not found. Run '$0 web' first."
        exit 1
    fi
    
    if check_python; then
        print_info "Starting web server on http://localhost:8000"
        print_info "Game URL: http://localhost:8000/robban_planterar.html"
        print_info "Press Ctrl+C to stop"
        cd build_web
        python3 -m http.server 8000
    else
        print_error "Python3 required for web server"
        exit 1
    fi
}

open_web() {
    build_web "$@"
    
    if check_python; then
        print_info "Opening game in browser..."
        cd build_web
        
        # Start server in background
        python3 -m http.server 8000 > /dev/null 2>&1 &
        SERVER_PID=$!
        
        # Wait a moment for server to start
        sleep 2
        
        # Open browser
        if command -v xdg-open &> /dev/null; then
            xdg-open http://localhost:8000/robban_planterar.html
        elif command -v open &> /dev/null; then
            open http://localhost:8000/robban_planterar.html
        elif command -v python3 &> /dev/null; then
            python3 -c "import webbrowser; webbrowser.open('http://localhost:8000/robban_planterar.html')"
        else
            print_info "Please open http://localhost:8000/robban_planterar.html in your browser"
        fi
        
        print_info "Server running with PID: $SERVER_PID"
        print_info "Press Ctrl+C to stop server"
        
        # Wait for interrupt
        trap "kill $SERVER_PID 2>/dev/null; exit 0" INT
        wait $SERVER_PID
    fi
}

clean_build() {
    print_info "Cleaning build directories..."
    rm -rf build_native build_web
    print_success "Clean complete!"
}

# Main script logic
print_header

if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

# Parse arguments
BUILD_TYPE="Debug"
ACTION=""
PLATFORM="native"

while [[ $# -gt 0 ]]; do
    case $1 in
        native)
            PLATFORM="native"
            shift
            ;;
        web)
            PLATFORM="web"
            shift
            ;;
        release)
            BUILD_TYPE="Release"
            shift
            ;;
        debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        clean)
            ACTION="clean"
            shift
            ;;
        serve)
            ACTION="serve"
            shift
            ;;
        open)
            ACTION="open"
            shift
            ;;
        help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Execute actions
case $ACTION in
    clean)
        clean_build
        ;;
    serve)
        serve_web
        ;;
    open)
        open_web $BUILD_TYPE
        ;;
    *)
        case $PLATFORM in
            native)
                build_native $BUILD_TYPE
                ;;
            web)
                build_web $BUILD_TYPE
                ;;
        esac
        ;;
esac