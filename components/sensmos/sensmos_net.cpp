#include "sensmos_net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace esphome {
namespace sensmos {

// Trwały worker (tworzony RAZ) ze STATYCZNYM stosem — nie ma alokacji/zwalniania stosu
// per request, więc sterta się nie fragmentuje (to był powód crashy OOM po kilku godzinach).
// 8 KB stosu starcza na plain HTTP i na TLS (z dynamicznym buforem mbedTLS).
static const uint32_t STACK_WORDS = 2048;   // 2048 * 4B = 8 KB
static StackType_t  g_stack[STACK_WORDS];
static StaticTask_t g_tcb;
static QueueHandle_t g_queue = nullptr;

static void worker(void *) {
  for (;;) {
    SensmosJob *job = nullptr;
    if (xQueueReceive(g_queue, &job, portMAX_DELAY) == pdTRUE && job != nullptr) {
      job->run(job);   // blokujące HTTP + finish()/finish_body()
      delete job;      // mała struktura (~kilkadziesiąt B) — nie fragmentuje jak stos 6–10 KB
    }
  }
}

static void net_init() {
  if (g_queue != nullptr)
    return;
  g_queue = xQueueCreate(3, sizeof(SensmosJob *));
  if (g_queue != nullptr)
    xTaskCreateStatic(worker, "sensmos_net", STACK_WORDS, nullptr,
                      tskIDLE_PRIORITY + 1, g_stack, &g_tcb);
}

bool net_submit(SensmosJob *job) {
  net_init();   // wołane z pętli głównej (jednowątkowo) → bezpieczna leniwa inicjalizacja
  if (g_queue == nullptr)
    return false;
  return xQueueSend(g_queue, &job, 0) == pdTRUE;
}

}  // namespace sensmos
}  // namespace esphome
