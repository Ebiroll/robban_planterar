# 🌳 Robban Planterar - Multiplayer Guide

## Overview

The game now supports **serverless WebRTC multiplayer** using PeerJS! Players can connect directly to each other without needing your own server.

## How It Works

- **PeerJS**: Uses free PeerJS cloud for signaling (connecting players)
- **WebRTC**: Direct peer-to-peer connection for game data
- **No Server Needed**: Everything runs in the browser, no backend required!

## Building for Web with Multiplayer

### 1. Clean Build
```bash
cd src
rm -rf build_web
mkdir build_web
cd build_web
```

### 2. Configure with Emscripten
```bash
emcmake cmake ..
```

### 3. Build
```bash
emmake make
```

### 4. Serve the Game
```bash
python3 -m http.server 8000
```

### 5. Open in Browser
Navigate to: `http://localhost:8000/robban_planterar.html`

## How to Play Multiplayer

### Hosting a Game:
1. Click **"🎮 Host Game"** button
2. Wait for Room ID to appear (e.g., `abc123xyz456`)
3. **Share this Room ID** with your friend (via Discord, text, etc.)
4. Wait for them to join

### Joining a Game:
1. Click **"🔗 Join Game"** button
2. Enter the **Room ID** your friend shared
3. Click **"Join"**
4. You should connect and see each other in the game!

## Game Controls

- **WASD** or **Arrow Keys**: Move your character
- **P**: Switch between modes (Plant → Shoot → Chop)
- **SPACE**: Perform action
  - Plant mode: Plant a tree
  - Shoot mode: Fire a bullet
  - Chop mode: Chop down a mature tree

## Multiplayer Features

✅ Real-time player movement synchronization
✅ Action synchronization (planting, shooting, chopping)
✅ Mode changes sync across players
✅ Support for 2+ players in same room
✅ Automatic reconnection handling

## Troubleshooting

### "PeerJS not loaded" Error
- Make sure you're accessing via HTTP server (not `file://`)
- Check browser console for errors
- Ensure internet connection (needed for PeerJS cloud)

### Players Can't Connect
- **Check Room ID**: Make sure it's copied exactly (case-sensitive)
- **Firewall**: Ensure WebRTC isn't blocked
- **Browser**: Use Chrome, Firefox, or Edge (latest versions)
- **Network**: Some corporate networks block WebRTC

### Connection Drops
- **Refresh**: Both players refresh and try again
- **New Room**: Host creates a new room with fresh ID
- **Network**: Check if either player's internet is unstable

## Technical Details

### Architecture
```
┌─────────────┐         ┌─────────────┐
│   Player 1  │         │   Player 2  │
│             │         │             │
│  C++ Game   │         │  C++ Game   │
│  (WASM)     │         │  (WASM)     │
│      ↕      │         │      ↕      │
│  JS Bridge  │         │  JS Bridge  │
│      ↕      │         │      ↕      │
│   PeerJS    │←─────→│   PeerJS    │
└─────────────┘         └─────────────┘
        ↓                       ↓
        └───────────────────────┘
                  ↓
        PeerJS Cloud (Signaling)
        Free • No Account Needed
```

### Message Types
- `PLAYER_MOVE`: Position and status updates
- `PLAYER_ACTION`: Planting, shooting, chopping
- `PLAYER_MODE_CHANGE`: Switching between modes

### Files
- [`peer_network.js`](src/peer_network.js): JavaScript PeerJS wrapper
- [`NetworkManager.cpp`](src/NetworkManager.cpp): C++ networking bridge
- [`shell_multiplayer.html`](src/shell_multiplayer.html): Custom HTML with PeerJS UI

## Future Improvements

- [ ] Add player names/nicknames
- [ ] Show player list in UI
- [ ] Add chat functionality
- [ ] Implement game state synchronization for late joiners
- [ ] Add reconnection with same Room ID
- [ ] Host migration when host leaves

## Performance Tips

- **Connection Quality**: Best on same network (LAN) or close geographic locations
- **Player Count**: Tested with 2-4 players; more may increase latency
- **Bandwidth**: Each connection uses ~100-500 KB/s depending on activity

## Privacy & Security

- ✅ **No Server Data**: All game data stays between players
- ✅ **No Account Required**: No registration or email needed
- ✅ **Encrypted**: WebRTC connections are encrypted by default
- ℹ️ **PeerJS Cloud**: Only used for initial connection setup

## Support

For issues or questions, check:
1. Browser console (F12) for error messages
2. Network tab to see connection status
3. This README for common solutions

---

**Happy Planting! 🌳**