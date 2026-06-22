#pragma once

namespace esphome {
namespace sensmos {

// Tylko JEDNO połączenie TLS naraz w całym komponencie (publish + wszystkie readbacki).
// ESP32 + WiFi + BLE + mbedTLS(cert bundle) = mało RAM; równoległe handshake'i → alloc fail (-0x7F00).
// acquire() wołane z pętli głównej (jednowątkowo), release() z taska po zakończeniu HTTP.
bool net_acquire();
void net_release();

}  // namespace sensmos
}  // namespace esphome
