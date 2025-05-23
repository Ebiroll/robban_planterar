#!/bin/bash

# Robban Planterar Dependency Installation Script
# Automatically installs required system dependencies

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}    Robban Planterar Dependencies    ${NC}"
    echo -e "${BLUE}======================================${NC}"
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

detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            echo "ubuntu"
        elif command -v yum &> /dev/null; then
            echo "rhel"
        elif command -v pacman &> /dev/null; then
            echo "arch"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

install_ubuntu_deps() {
    print_info "Installing dependencies for Ubuntu/Debian..."
    
    # Update package list
    print_info "Updating package list..."
    sudo apt-get update
    
    # Install build tools
    print_info "Installing build tools..."
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config
    
    # Install X11 and graphics libraries
    print_info "Installing X11 and graphics libraries..."
    sudo apt-get install -y \
        libx11-dev \
        libxrandr-dev \
        libxcursor-dev \
        libxi-dev \
        libxinerama-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libasound2-dev \
        libpulse-dev
    
    # Install optional Python for web server
    print_info "Installing Python for web server..."
    sudo apt-get install -y python3
    
    print_success "Ubuntu/Debian dependencies installed!"
}

install_rhel_deps() {
    print_info "Installing dependencies for RHEL/CentOS/Fedora..."
    
    # Detect package manager
    if command -v dnf &> /dev/null; then
        PKG_MGR="dnf"
    else
        PKG_MGR="yum"
    fi
    
    # Install build tools
    print_info "Installing build tools..."
    sudo $PKG_MGR install -y \
        gcc-c++ \
        cmake \
        git \
        pkgconfig
    
    # Install X11 and graphics libraries
    print_info "Installing X11 and graphics libraries..."
    sudo $PKG_MGR install -y \
        libX11-devel \
        libXrandr-devel \
        libXcursor-devel \
        libXi-devel \
        libXinerama-devel \
        mesa-libGL-devel \
        mesa-libGLU-devel \
        alsa-lib-devel \
        pulseaudio-libs-devel
    
    # Install optional Python for web server
    print_info "Installing Python for web server..."
    sudo $PKG_MGR install -y python3
    
    print_success "RHEL/CentOS/Fedora dependencies installed!"
}

install_arch_deps() {
    print_info "Installing dependencies for Arch Linux..."
    
    # Update package database
    print_info "Updating package database..."
    sudo pacman -Sy
    
    # Install dependencies
    print_info "Installing build tools and libraries..."
    sudo pacman -S --needed \
        base-devel \
        cmake \
        git \
        pkgconf \
        libx11 \
        libxrandr \
        libxcursor \
        libxi \
        libxinerama \
        mesa \
        alsa-lib \
        libpulse \
        python
    
    print_success "Arch Linux dependencies installed!"
}

install_macos_deps() {
    print_info "Installing dependencies for macOS..."
    
    # Check for Homebrew
    if ! command -v brew &> /dev/null; then
        print_error "Homebrew not found. Please install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    
    # Install CMake and Git
    print_info "Installing build tools..."
    brew install cmake git
    
    # Python should already be available on macOS
    print_info "Python should already be available on macOS"
    
    print_success "macOS dependencies installed!"
}

install_emscripten() {
    print_info "Installing Emscripten SDK for web builds..."
    
    if [ -d "emsdk" ]; then
        print_warning "Emscripten SDK directory already exists. Updating..."
        cd emsdk
        git pull
    else
        print_info "Cloning Emscripten SDK..."
        git clone https://github.com/emscripten-core/emsdk.git
        cd emsdk
    fi
    
    print_info "Installing latest Emscripten..."
    ./emsdk install latest
    
    print_info "Activating Emscripten..."
    ./emsdk activate latest
    
    cd ..
    
    print_success "Emscripten SDK installed!"
    print_warning "Remember to run 'source emsdk/emsdk_env.sh' before building web version"
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --system        Install system dependencies only"
    echo "  --emscripten    Install Emscripten SDK only"
    echo "  --all           Install everything (default)"
    echo "  --help          Show this help message"
    echo ""
    echo "This script will install the required dependencies for building Robban Planterar."
    echo "Supported systems: Ubuntu/Debian, RHEL/CentOS/Fedora, Arch Linux, macOS"
}

# Main script
print_header

# Parse arguments
INSTALL_SYSTEM=true
INSTALL_EMSCRIPTEN=true

if [ $# -gt 0 ]; then
    case $1 in
        --system)
            INSTALL_SYSTEM=true
            INSTALL_EMSCRIPTEN=false
            ;;
        --emscripten)
            INSTALL_SYSTEM=false
            INSTALL_EMSCRIPTEN=true
            ;;
        --all)
            INSTALL_SYSTEM=true
            INSTALL_EMSCRIPTEN=true
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
fi

# Install system dependencies
if [ "$INSTALL_SYSTEM" = true ]; then
    OS=$(detect_os)
    print_info "Detected OS: $OS"
    
    case $OS in
        ubuntu)
            install_ubuntu_deps
            ;;
        rhel)
            install_rhel_deps
            ;;
        arch)
            install_arch_deps
            ;;
        macos)
            install_macos_deps
            ;;
        *)
            print_error "Unsupported operating system: $OS"
            print_info "Please install the following manually:"
            echo "  - CMake 3.15+"
            echo "  - Git"
            echo "  - C++ compiler (GCC/Clang)"
            echo "  - X11 development libraries (Linux only):"
            echo "    - libx11-dev, libxrandr-dev, libxcursor-dev"
            echo "    - libxi-dev, libxinerama-dev"
            echo "  - OpenGL development libraries"
            echo "  - Audio libraries (ALSA/PulseAudio on Linux)"
            exit 1
            ;;
    esac
fi

# Install Emscripten
if [ "$INSTALL_EMSCRIPTEN" = true ]; then
    echo ""
    print_info "Do you want to install Emscripten SDK for web builds? [y/N]"
    read -r response
    case $response in
        [yY][eE][sS]|[yY])
            install_emscripten
            ;;
        *)
            print_info "Skipping Emscripten installation"
            ;;
    esac
fi

echo ""
print_success "Dependency installation complete!"
print_info "You can now build the game with:"
echo "  ./build.sh native    # For native builds"
echo "  ./build.sh web       # For web builds (requires Emscripten)"

if [ "$INSTALL_EMSCRIPTEN" = true ] && [ -d "emsdk" ]; then
    echo ""
    print_warning "For web builds, remember to activate Emscripten first:"
    echo "  source emsdk/emsdk_env.sh"
fi
