// PeerJS WebRTC Networking for Robban Planterar
// This bridges between Emscripten/WASM and PeerJS for multiplayer

// Emscripten library integration
mergeInto(LibraryManager.library, {
    // Define PeerNetworkState in library scope
    $PeerNetworkState__postset: 'PeerNetworkState = { peer: null, connections: {}, roomId: null, isHost: false };',
    $PeerNetworkState: {},

    // Initialize PeerJS networking
    JS_InitPeerNetwork__deps: ['$PeerNetworkState'],
    JS_InitPeerNetwork: function() {
        console.log('[PeerNetwork] JS_InitPeerNetwork called');

        if (typeof Peer === 'undefined') {
            console.error('[PeerNetwork] PeerJS not loaded! Include peerjs library in HTML.');
            return 0;
        }

        console.log('[PeerNetwork] Initializing...');

        // Create peer with public PeerJS cloud server
        PeerNetworkState.peer = new Peer({
            config: {
                iceServers: [
                    { urls: 'stun:stun.l.google.com:19302' },
                    { urls: 'stun:stun1.l.google.com:19302' }
                ]
            }
        });

        PeerNetworkState.peer.on('open', function(id) {
            console.log('[PeerNetwork] Peer ID:', id);
            PeerNetworkState.roomId = id;

            // Notify C++ code that we're ready
            if (Module._OnPeerReady) {
                var idPtr = Module.allocateUTF8(id);
                Module._OnPeerReady(idPtr);
                Module._free(idPtr);
            }
            if(window.updateRoomId) {
                window.updateRoomId(id);
            }
        });

        PeerNetworkState.peer.on('connection', function(conn) {
            console.log('[PeerNetwork] Incoming connection from:', conn.peer);

            // Setup connection handlers inline
            conn.on('open', function() {
                console.log('[PeerNetwork] Connection opened with:', conn.peer);
                PeerNetworkState.connections[conn.peer] = conn;

                // Notify C++ code
                if (Module._OnPlayerJoined) {
                    var peerIdPtr = Module.allocateUTF8(conn.peer);
                    Module._OnPlayerJoined(peerIdPtr);
                    Module._free(peerIdPtr);
                }
            });

            conn.on('data', function(data) {
                console.log('[PeerNetwork] Received:', data);

                // Notify C++ code with message
                if (Module._OnNetworkMessage) {
                    var dataStr = JSON.stringify(data);
                    var dataPtr = Module.allocateUTF8(dataStr);
                    Module._OnNetworkMessage(dataPtr);
                    Module._free(dataPtr);
                }
            });

            conn.on('close', function() {
                console.log('[PeerNetwork] Connection closed with:', conn.peer);
                delete PeerNetworkState.connections[conn.peer];

                // Notify C++ code
                if (Module._OnPlayerLeft) {
                    var peerIdPtr = Module.allocateUTF8(conn.peer);
                    Module._OnPlayerLeft(peerIdPtr);
                    Module._free(peerIdPtr);
                }
            });

            conn.on('error', function(err) {
                console.error('[PeerNetwork] Connection error:', err);
            });
        });

        PeerNetworkState.peer.on('error', function(err) {
            console.error('[PeerNetwork] Error:', err);
        });

        return 1;
    },

    // Create room (host)
    JS_CreateRoom__deps: ['$PeerNetworkState'],
    JS_CreateRoom: function() {
        console.log('[PeerNetwork] JS_CreateRoom called');
        PeerNetworkState.isHost = true;
        // Room is automatically created when peer initializes
        // Just return success
        return 1;
    },

    // Join room
    JS_JoinRoom__deps: ['$PeerNetworkState'],
    JS_JoinRoom: function(roomIdPtr) {
        var roomId = UTF8ToString(roomIdPtr);
        console.log('[PeerNetwork] Connecting to:', roomId);

        if (!PeerNetworkState.peer) {
            console.error('[PeerNetwork] Peer not initialized!');
            return 0;
        }

        var conn = PeerNetworkState.peer.connect(roomId, {
            reliable: true
        });

        // Setup connection handlers
        conn.on('open', function() {
            console.log('[PeerNetwork] Connection opened with:', conn.peer);
            PeerNetworkState.connections[conn.peer] = conn;

            // Notify C++ code
            if (Module._OnPlayerJoined) {
                var peerIdPtr = Module.allocateUTF8(conn.peer);
                Module._OnPlayerJoined(peerIdPtr);
                Module._free(peerIdPtr);
            }
        });

        conn.on('data', function(data) {
            console.log('[PeerNetwork] Received:', data);

            // Notify C++ code with message
            if (Module._OnNetworkMessage) {
                var dataStr = JSON.stringify(data);
                var dataPtr = Module.allocateUTF8(dataStr);
                Module._OnNetworkMessage(dataPtr);
                Module._free(dataPtr);
            }
        });

        conn.on('close', function() {
            console.log('[PeerNetwork] Connection closed with:', conn.peer);
            delete PeerNetworkState.connections[conn.peer];

            // Notify C++ code
            if (Module._OnPlayerLeft) {
                var peerIdPtr = Module.allocateUTF8(conn.peer);
                Module._OnPlayerLeft(peerIdPtr);
                Module._free(peerIdPtr);
            }
        });

        conn.on('error', function(err) {
            console.error('[PeerNetwork] Connection error:', err);
        });

        return 1;
    },

    // Send message to all peers
    JS_BroadcastMessage__deps: ['$PeerNetworkState'],
    JS_BroadcastMessage: function(messagePtr) {
        var message = UTF8ToString(messagePtr);
        console.log('[PeerNetwork] Broadcasting message:', message);

        var messageObj = JSON.parse(message);

        for (var peerId in PeerNetworkState.connections) {
            if (PeerNetworkState.connections.hasOwnProperty(peerId)) {
                try {
                    PeerNetworkState.connections[peerId].send(messageObj);
                    console.log('[PeerNetwork] Sent to peer:', peerId);
                } catch (e) {
                    console.error('[PeerNetwork] Error sending to', peerId, ':', e);
                }
            }
        }
    },

    // Send message to a specific peer
    JS_SendMessageTo__deps: ['$PeerNetworkState'],
    JS_SendMessageTo: function(peerIdPtr, messagePtr) {
        var peerId = UTF8ToString(peerIdPtr);
        var message = UTF8ToString(messagePtr);
        console.log('[PeerNetwork] Sending message to ' + peerId + ':', message);

        var messageObj = JSON.parse(message);

        if (PeerNetworkState.connections.hasOwnProperty(peerId)) {
            try {
                PeerNetworkState.connections[peerId].send(messageObj);
                console.log('[PeerNetwork] Sent to peer:', peerId);
            } catch (e) {
                console.error('[PeerNetwork] Error sending to', peerId, ':', e);
            }
        } else {
            console.warn('[PeerNetwork] Could not send to peer, no connection:', peerId);
        }
    },

    // Get room ID
    JS_GetRoomId__deps: ['$PeerNetworkState'],
    JS_GetRoomId: function(buffer, bufferSize) {
        var roomId = PeerNetworkState.roomId;
        if (roomId) {
            stringToUTF8(roomId, buffer, bufferSize);
            return 1;
        }
        return 0;
    },

    // Get connection count
    JS_GetConnectionCount__deps: ['$PeerNetworkState'],
    JS_GetConnectionCount: function() {
        return Object.keys(PeerNetworkState.connections).length;
    },

    // Disconnect
    JS_DisconnectPeer__deps: ['$PeerNetworkState'],
    JS_DisconnectPeer: function() {
        for (var peerId in PeerNetworkState.connections) {
            if (PeerNetworkState.connections.hasOwnProperty(peerId)) {
                PeerNetworkState.connections[peerId].close();
            }
        }
        PeerNetworkState.connections = {};

        if (PeerNetworkState.peer) {
            PeerNetworkState.peer.destroy();
            PeerNetworkState.peer = null;
        }
    }
});

// Also expose to window for HTML access
if (typeof window !== 'undefined') {
    window.PeerNetwork = LibraryManager.library.PeerNetworkState;
}