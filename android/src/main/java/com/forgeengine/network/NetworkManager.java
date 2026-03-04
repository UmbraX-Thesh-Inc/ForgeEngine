package com.forgeengine.network;

// ============================================================
//  ForgeEngine – NetworkManager.java
//  Basic multiplayer using raw TCP/UDP sockets (KryoNet style).
//  Editor can preview network state + configure server settings.
//  Production: swap in KryoNet or Netty as dependency.
// ============================================================

import java.io.*;
import java.net.*;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

public class NetworkManager {

    // ── Packet types ──────────────────────────────────────────
    public static final byte PKT_CONNECT    = 1;
    public static final byte PKT_DISCONNECT = 2;
    public static final byte PKT_TRANSFORM  = 3;
    public static final byte PKT_SPAWN      = 4;
    public static final byte PKT_DESPAWN    = 5;
    public static final byte PKT_CUSTOM     = 6;
    public static final byte PKT_PING       = 7;
    public static final byte PKT_PONG       = 8;
    public static final byte PKT_CHAT       = 9;

    // ── Packet ────────────────────────────────────────────────
    public static class Packet {
        public byte   type;
        public int    peerId;
        public byte[] payload;
        public Packet(byte type, int peerId, byte[] payload) {
            this.type=type; this.peerId=peerId; this.payload=payload;
        }
    }

    // ── Peer state ────────────────────────────────────────────
    public static class Peer {
        public int    id;
        public String address;
        public int    port;
        public long   lastPing;
        public float  rtt;        // round-trip ms
        public Socket socket;
        public DataOutputStream out;
        public DataInputStream  in;
    }

    // ── Mode ──────────────────────────────────────────────────
    public enum Mode { OFFLINE, SERVER, CLIENT }

    private Mode          mMode    = Mode.OFFLINE;
    private ServerSocket  mServer;
    private Peer          mSelf    = new Peer();
    private final Map<Integer, Peer> mPeers = new ConcurrentHashMap<>();
    private final AtomicInteger mPeerIdGen = new AtomicInteger(2); // 1=self
    private final BlockingQueue<Packet> mIncoming = new LinkedBlockingQueue<>(1024);
    private final ExecutorService mPool = Executors.newCachedThreadPool();
    private volatile boolean mRunning = false;

    // ── Listener ──────────────────────────────────────────────
    private Consumer<Packet> mPacketListener;
    private Consumer<Integer> mConnectListener;
    private Consumer<Integer> mDisconnectListener;

    public void setPacketListener(Consumer<Packet> l)      { mPacketListener = l; }
    public void setConnectListener(Consumer<Integer> l)    { mConnectListener = l; }
    public void setDisconnectListener(Consumer<Integer> l) { mDisconnectListener = l; }

    // ─────────────────────────────────────────────────────────
    //  Host server
    // ─────────────────────────────────────────────────────────
    public boolean startServer(int port, int maxPlayers) {
        try {
            mServer = new ServerSocket(port);
            mMode   = Mode.SERVER;
            mRunning = true;
            mSelf.id = 1;

            mPool.submit(() -> {
                while (mRunning) {
                    try {
                        Socket client = mServer.accept();
                        int id = mPeerIdGen.getAndIncrement();
                        Peer p = buildPeer(id, client);
                        mPeers.put(id, p);
                        if (mConnectListener != null) mConnectListener.accept(id);
                        startReceiving(p);
                        // Send welcome
                        sendTo(id, PKT_CONNECT, intToBytes(id));
                    } catch (Exception ignored) {}
                }
            });
            return true;
        } catch (IOException e) { return false; }
    }

    // ─────────────────────────────────────────────────────────
    //  Connect as client
    // ─────────────────────────────────────────────────────────
    public boolean connectToServer(String host, int port, int timeoutMs) {
        try {
            Socket sock = new Socket();
            sock.connect(new InetSocketAddress(host, port), timeoutMs);
            mMode    = Mode.CLIENT;
            mRunning = true;
            Peer server = buildPeer(1, sock);
            mPeers.put(1, server);
            startReceiving(server);
            sendTo(1, PKT_CONNECT, intToBytes(0));
            return true;
        } catch (IOException e) { return false; }
    }

    // ─────────────────────────────────────────────────────────
    //  Disconnect
    // ─────────────────────────────────────────────────────────
    public void disconnect() {
        mRunning = false;
        for (Peer p : mPeers.values()) closePeer(p);
        mPeers.clear();
        if (mServer != null) try { mServer.close(); } catch(IOException ignored){}
        mMode = Mode.OFFLINE;
    }

    // ─────────────────────────────────────────────────────────
    //  Send
    // ─────────────────────────────────────────────────────────
    public void sendTo(int peerId, byte type, byte[] data) {
        Peer p = mPeers.get(peerId);
        if (p == null || p.out == null) return;
        mPool.submit(() -> {
            try {
                synchronized(p.out) {
                    p.out.writeByte(type);
                    p.out.writeInt(data != null ? data.length : 0);
                    if (data != null) p.out.write(data);
                    p.out.flush();
                }
            } catch (IOException e) { handleDisconnect(peerId); }
        });
    }

    public void broadcast(byte type, byte[] data) {
        for (int id : mPeers.keySet()) sendTo(id, type, data);
    }

    // ── Transform broadcast (main hot-path) ───────────────────
    public void broadcastTransform(int entityId,
                                    float tx, float ty, float tz,
                                    float rx, float ry, float rz, float rw) {
        ByteBuffer buf = ByteBuffer.allocate(32);
        buf.putInt(entityId);
        buf.putFloat(tx); buf.putFloat(ty); buf.putFloat(tz);
        buf.putFloat(rx); buf.putFloat(ry); buf.putFloat(rz); buf.putFloat(rw);
        broadcast(PKT_TRANSFORM, buf.array());
    }

    // ── Chat ──────────────────────────────────────────────────
    public void sendChat(String msg) {
        try {
            byte[] data = msg.getBytes("UTF-8");
            if (mMode == Mode.SERVER) broadcast(PKT_CHAT, data);
            else sendTo(1, PKT_CHAT, data);
        } catch (Exception ignored) {}
    }

    // ─────────────────────────────────────────────────────────
    //  Poll incoming packets (call from game update loop)
    // ─────────────────────────────────────────────────────────
    public Packet pollPacket() {
        return mIncoming.poll();
    }

    public void processAll() {
        Packet pkt;
        while ((pkt = mIncoming.poll()) != null) {
            if (mPacketListener != null) mPacketListener.accept(pkt);
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Ping
    // ─────────────────────────────────────────────────────────
    public void pingAll() {
        byte[] ts = longToBytes(System.currentTimeMillis());
        for (int id : mPeers.keySet()) sendTo(id, PKT_PING, ts);
    }

    // ─────────────────────────────────────────────────────────
    //  Status JSON (for editor panel)
    // ─────────────────────────────────────────────────────────
    public String getStatusJSON() {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"mode\":\"").append(mMode).append("\"")
          .append(",\"selfId\":").append(mSelf.id)
          .append(",\"peers\":[");
        boolean first = true;
        for (Peer p : mPeers.values()) {
            if (!first) sb.append(",");
            first = false;
            sb.append("{\"id\":").append(p.id)
              .append(",\"addr\":\"").append(p.address).append("\"")
              .append(",\"rtt\":").append(p.rtt)
              .append("}");
        }
        sb.append("]}");
        return sb.toString();
    }

    public Mode getMode()       { return mMode; }
    public int  getPeerCount()  { return mPeers.size(); }

    // ─────────────────────────────────────────────────────────
    //  Internal helpers
    // ─────────────────────────────────────────────────────────
    private Peer buildPeer(int id, Socket sock) throws IOException {
        Peer p = new Peer();
        p.id      = id;
        p.socket  = sock;
        p.address = sock.getInetAddress().getHostAddress();
        p.port    = sock.getPort();
        p.out     = new DataOutputStream(new BufferedOutputStream(sock.getOutputStream()));
        p.in      = new DataInputStream(new BufferedInputStream(sock.getInputStream()));
        return p;
    }

    private void startReceiving(Peer p) {
        mPool.submit(() -> {
            while (mRunning && !p.socket.isClosed()) {
                try {
                    byte type = p.in.readByte();
                    int  len  = p.in.readInt();
                    byte[] data = new byte[len];
                    if (len > 0) p.in.readFully(data);
                    Packet pkt = new Packet(type, p.id, data);

                    if (type == PKT_PING) {
                        sendTo(p.id, PKT_PONG, data);
                    } else if (type == PKT_PONG) {
                        long sent = bytesToLong(data);
                        p.rtt = (System.currentTimeMillis() - sent) / 2f;
                    } else if (type == PKT_DISCONNECT) {
                        handleDisconnect(p.id); break;
                    } else {
                        // Server: relay to others
                        if (mMode == Mode.SERVER && type == PKT_TRANSFORM) {
                            for (int rid : mPeers.keySet())
                                if (rid != p.id) sendTo(rid, type, data);
                        }
                        mIncoming.offer(pkt);
                    }
                } catch (Exception e) { handleDisconnect(p.id); break; }
            }
        });
    }

    private void handleDisconnect(int peerId) {
        Peer p = mPeers.remove(peerId);
        if (p != null) {
            closePeer(p);
            if (mDisconnectListener != null) mDisconnectListener.accept(peerId);
        }
    }

    private void closePeer(Peer p) {
        try { p.socket.close(); } catch(Exception ignored){}
    }

    private byte[] intToBytes(int v) {
        return ByteBuffer.allocate(4).putInt(v).array();
    }
    private byte[] longToBytes(long v) {
        return ByteBuffer.allocate(8).putLong(v).array();
    }
    private long bytesToLong(byte[] b) {
        return ByteBuffer.wrap(b).getLong();
    }
}
