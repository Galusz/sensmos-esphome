#include "sensmos_net.h"

namespace esphome {
namespace sensmos {

// acquire() jest wołane wyłącznie z pętli głównej ESPHome (update() komponentów) — jednowątkowo,
// więc zwykły check-and-set wystarcza. release() ustawia tylko false (z taska po zakończeniu HTTP).
static volatile bool g_busy = false;

bool net_acquire() {
  if (g_busy)
    return false;
  g_busy = true;
  return true;
}

void net_release() { g_busy = false; }

}  // namespace sensmos
}  // namespace esphome
