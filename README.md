# robban_planterar
Vibe coding a simple game. Using claude 4.
Here is the original prompt:
Create a multiuser game called Robban planterar. The Idea is to have an area covered by squares. In each square a player can plant a tree. Each player can either plant, fire a gun or chop down trees. The player switches mode by pressing a key p. The tree will grow slowly and each player have different colours for the trees. Before the tree is fully grown it can be eaten by rabbits or deer. The users can kill the animals or other players with the gun. When a person is killed there is a grave on that square with the colour of that player. The trees grows slowly but when they are big 1/2 full age the animals cannot eat it but the players can cut them down. You get points for cutting down trees or killing an animal. Players get negative points for shoting another player, but the player immediately respawns in one random corner when killed. You should be able to control movemets with the wasd keys or arrows. Animals are spawned at all times randomly. At startup there are some shrubbery in the forrest that the animals can eat but when a tree is planted the shrubbery is removed. I prefer to have this game use raylib and C++. The networking code can preferably use webrtc. Use the graphics below for inspiration.

# Build/Install
Building in 

# Robban Planterar 🌲

A multiplayer tree planting and survival game inspired by Minecraft's pixelated aesthetic. Plant trees, defend against animals, and compete with other players in this strategic forestry game!

## Game Overview

In **Robban Planterar**, players compete to plant and grow trees while managing threats from wildlife and other players. Each player has three modes of operation:

- **🌱 Plant Mode**: Plant tree seedlings that grow over time
- **🔫 Shoot Mode**: Eliminate animals and other players  
- **🪓 Chop Mode**: Harvest mature trees for points

## Features

### Core Gameplay
- **Grid-based world** with 40x30 cells
- **Tree growth system** - seedlings grow to mature trees over time
- **Wildlife ecosystem** - rabbits and deer spawn and eat young vegetation
- **Player combat** - shoot other players (they respawn with penalties)
- **Scoring system** - gain points for harvesting trees and eliminating animals
- **Colored player identification** - each player has unique tree and grave colors

### Multiplayer Networking
- **WebRTC-based networking** for real-time multiplayer
- **Host or join rooms** for multiplayer sessions
- **Real-time synchronization** of player actions and game state
- **Up to 8 players** with unique color coding

### Visual Elements
- **Pixel art style** inspired by the provided Minecraft-like graphics
- **Color-coded trees** showing ownership
- **Animated wildlife** that moves and feeds
- **Player graves** when eliminated
- **Growth visualization** for developing trees

## Controls

### Movement
- **WASD** or **Arrow Keys**: Move your character
- **Mouse Click**: Perform action at target location

### Game Actions
- **P**: Switch between Plant/Shoot/Chop modes
- **H**: Host a multiplayer game (single-player mode)
- **J**: Join a multiplayer game (single-player mode)

## Building the Game

### Prerequisites
- **C++17** compatible compiler
- **CMake 3.15+**
- **Raylib** graphics library
- **Git** for cloning

### Build Instructions

1. **Install Raylib**:
   ```bash
   # On Ubuntu/Debian
   sudo apt-get install libraylib-dev
   
   # On macOS with Homebrew
   brew install raylib
   
   # On Windows, download from: https://www.raylib.com/
   ```

2. **Clone and Build**:
   ```bash
   git clone <repository-url>
   cd robban-planterar
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Run the Game**:
   ```bash
   ./robban_planterar
   ```

### Optional WebRTC Support

For full multiplayer functionality, enable WebRTC:

```bash
cmake -DENABLE_WEBRTC=ON ..
make
```

*Note: This requires libwebrtc to be installed separately.*

## How to Play

### Single Player
1. Launch the game
2. Use WASD to move around the forest
3. Press P to cycle through modes (Plant/Shoot/Chop)
4. Click to perform actions:
   - **Plant**: Place seedlings on empty ground
   - **Shoot**: Eliminate animals or other players
   - **Chop**: Harvest mature trees
5. Watch out for animals that eat young trees!

### Multiplayer
1. **Host a game**: Press H to create a room
2. **Join a game**: Press J to join an existing room
3. Share room ID with friends
4. Compete for the highest score!

## Game Mechanics

### Tree Growth
- **Seedlings**: Vulnerable to animals, grow slowly
- **Young Trees**: Still edible by animals until 50% grown
- **Mature Trees**: Safe from animals, can be harvested

### Wildlife Behavior
- Animals spawn randomly across the map
- They seek out and consume vegetation
- Rabbits and deer have different movement patterns
- Eliminating animals awards points

### Scoring System
- **+10 points**: Harvesting a mature tree
- **+5 points**: Eliminating an animal
- **-5 points**: Shooting another player (but they respawn)

### Strategy Tips
- **Protect young trees** by eliminating nearby animals
- **Plant in groups** for better survival rates
- **Use terrain** to your advantage when hunting
- **Balance offense and defense** - don't neglect tree planting!

## File Structure

```
robban-planterar/
├── main.cpp              # Main game loop and logic
├── NetworkManager.h      # Networking interface
├── NetworkManager.cpp    # Networking implementation  
├── CMakeLists.txt        # Build configuration
└── README.md            # This file
```

## Technical Details

### Graphics
- **Raylib** for 2D rendering and input handling
- **Pixel-perfect** grid-based rendering
- **Color-coded** visual elements for player identification

### Networking
- **WebRTC** for peer-to-peer multiplayer (when enabled)
- **Message-based** synchronization system
- **Client-server** architecture with host authority

### Performance
- **60 FPS** target frame rate
- **Efficient** grid-based collision detection
- **Optimized** rendering for large game worlds

## Known Issues & Future Improvements

### Current Limitations
- WebRTC implementation is simplified (requires full integration)
- Room joining uses hardcoded room IDs
- Limited to 8 players due to color constraints
- No persistent game saves

### Planned Features
- **Sound effects** and background music
- **Power-ups** and special abilities
- **Larger maps** with different biomes
- **Tournament mode** with brackets
- **Statistics tracking** and leaderboards

## Contributing

Contributions are welcome! Areas for improvement:

1. **Full WebRTC integration** with proper signaling server
2. **Enhanced AI** for animal behavior
3. **Additional game modes** and objectives
4. **Visual effects** and animations
5. **Mobile platform** support

## License

This project is open source. Feel free to modify and distribute according to your needs.

---

**Happy planting!** 🌲🎮