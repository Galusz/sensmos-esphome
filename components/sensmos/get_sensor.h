#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <string>

namespace esphome {
namespace sensmos {

// Czyta JEDNĄ opublikowaną encję innego noda Sensmos (real lub programowego) i wystawia
// ją jako sensor ESPHome. To podgląd — odpytuje rzadko (domyślnie 5 min). GET leci w osobnym
// tasku (nie dławi pętli/BLE); parsowanie + publish robimy w loop() (logger nie jest thread-safe).
class SensmosGetSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_target(const std::string &device_id, const std::string &entity) {
    this->device_id_ = device_id;
    this->entity_ = entity;
  }
  void set_insecure(bool v) { this->insecure_ = v; }  // true → http:// (bez TLS)

  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // wołane z taska: zapis surowej odpowiedzi do bufora (publish/parsowanie w loop())
  void finish_body(const std::string &body, bool http_ok) {
    this->body_ = body;
    this->http_ok_ = http_ok;
    this->have_body_ = true;  // ustaw NA KOŃCU
    this->busy_ = false;
  }

  const std::string &device_id() const { return this->device_id_; }
  const std::string &entity() const { return this->entity_; }

 protected:
  std::string device_id_;
  std::string entity_;
  std::string body_;            // surowa odpowiedź z taska → konsumowana w loop()
  bool insecure_{false};        // http zamiast https (omija TLS/cert → mało RAM)
  volatile bool busy_{false};   // GET trwa w tasku → nie startuj kolejnego
  volatile bool pending_{false};// update() zaznacza chęć pobrania; start w loop() gdy TLS wolne
  volatile bool have_body_{false};
  volatile bool http_ok_{false};
};

}  // namespace sensmos
}  // namespace esphome
