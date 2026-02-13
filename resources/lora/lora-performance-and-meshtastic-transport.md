# LoRa Performance Optimization & Meshtastic Multi-Transport Routing

## LoRa Performance Methods

### RF-Level Tuning
- **Spreading Factor (SF)**: Lower SF (e.g., SF7) = faster data rate, shorter airtime, less range. Higher SF = more range, slower.
- **Adaptive Data Rate (ADR)**: LoRaWAN dynamically adjusts SF, TX power, and bandwidth per link quality.
- **Bandwidth**: 125/250/500 kHz options. Wider = more throughput, less sensitivity/range.
- **Forward Error Correction (FEC)**: Configurable coding rates (4/5 through 4/8) trade redundancy for robustness.

### Packet Prioritization
- **Custom application-layer header**: Prepend a priority byte to payload. Network/application server inspects and queue-jumps high-priority packets.
- **FPort-based routing**: Different FPort values route packets to different processing pipelines with different priority levels.
- **LoRaWAN Class differentiation**: Class A (low power, high latency), Class B (scheduled windows), Class C (always listening, near-real-time).

### Queuing & Scheduling
- Priority queues on gateway/network server for downlink.
- Time-slotted approach (Class B beaconing) for guaranteed windows to critical devices.
- MAC command scheduling: network server tells high-priority nodes when to transmit, reducing collisions.

### Throughput Boosters
- **Channel hopping** across multiple frequencies reduces collision probability.
- **Payload compression** reduces airtime.
- **Redundant gateways** improve reception via spatial diversity.
- **Frequency multiplexing** — parallel channels on different frequencies multiply throughput.
- **Multi-radio channel bonding** (custom): split payload across simultaneous transmissions on different frequencies using multiple radio modules, reassemble at gateway. Not native to LoRa, but feasible.

---

## Meshtastic: Preferring WiFi/BT Over LoRa

### Current Transport Roles (Not Interchangeable)
- **Bluetooth**: Client app ↔ local node link only. Not node-to-node.
- **WiFi**: Web UI, MQTT uplink, and (v2.6+) experimental LAN meshing over UDP.
- **LoRa**: Primary node-to-node mesh transport.
- **Constraint**: ESP32 disables Bluetooth when WiFi is enabled. Can't run both.

### Key Limitation
Meshtastic's mesh algorithm is flood-based. There is no routing table tracking which transport can reach which node. No native logic exists to say "this node is reachable via WiFi, suppress LoRa."

### Meshtastic 2.6 UDP Meshing (Closest to the Goal)
- Nodes on the same WiFi/LAN can exchange mesh packets over UDP.
- Works **alongside** LoRa, not instead of it — no automatic LoRa suppression for LAN-reachable nodes.
- ESP32 only, experimental.

### Practical Workarounds Today

1. **UDP mesh + LOCAL_ONLY rebroadcast**: Co-located nodes on same WiFi with UDP meshing enabled. Set rebroadcast to `LOCAL_ONLY` so they don't waste LoRa airtime between each other.
2. **MQTT as WiFi-preferred backhaul**: WiFi-connected nodes uplink/downlink via MQTT, creating a parallel high-bandwidth path alongside LoRa.
3. **CLIENT_MUTE near a CLIENT_BASE**: Nearby nodes don't rebroadcast over LoRa. Combined with UDP meshing, the local cluster uses WiFi while only the base handles LoRa to the wider mesh.
4. **Custom firmware mod**: Modify `FloodingRouter.cpp` to check if a packet's destination is reachable over UDP/LAN before transmitting over LoRa. This is the "real" solution but requires firmware development.

### Community Status
- MeshCore (competing project) has an open feature request for this exact concept — using 2.4 GHz locally and LoRa only for distant traffic, specifically to avoid RF collisions at network edges.
- The idea is recognized as valuable but not fully implemented in either major LoRa mesh project yet.
