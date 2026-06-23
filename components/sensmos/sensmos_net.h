#pragma once

#include <string>

namespace esphome {
namespace sensmos {

// Jedno zadanie HTTP wykonywane przez WSPÓLNY, trwały worker (publish + readbacki).
// run() robi blokujące HTTP i dostarcza wynik (zdefiniowane w pliku komponentu — bez zależności krzyżowych).
struct SensmosJob {
  void (*run)(SensmosJob *);
  void *self;
  std::string url;
  std::string body;
};

// Kolejkuje zadanie do persistentnego workera (task tworzony raz, STATYCZNY stos → brak
// alokacji/zwalniania dużego stosu per request → brak fragmentacji sterty, brak crashy OOM).
// Zwraca false gdy kolejka pełna. Wołane z pętli głównej (jednowątkowo).
bool net_submit(SensmosJob *job);

}  // namespace sensmos
}  // namespace esphome
