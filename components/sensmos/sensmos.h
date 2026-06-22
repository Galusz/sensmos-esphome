#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>
#include <string>
#include <utility>

namespace esphome {
namespace sensmos {

// Wysyła wybrane sensory ESPHome na żywą mapę Sensmos (programowy / "virtual" node).
// Tożsamość = sha256(key); pierwszy POST auto-rejestruje node. Bez portfela i podpisów.
class SensmosComponent : public PollingComponent {
 public:
  void set_key(const std::string &k) { this->key_ = k; }
  void set_location(float lat, float lon) {
    this->lat_ = lat;
    this->lon_ = lon;
    this->has_loc_ = true;
  }
  void set_label(const std::string &l) { this->label_ = l; }
  void set_insecure(bool v) { this->insecure_ = v; }  // true → http:// (bez TLS, dla słabych nodów)
  void add_sensor(sensor::Sensor *s, const std::string &entity) {
    this->sensors_.emplace_back(s, entity);
  }

  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // wołane z taska wysyłki: zapis wyniku + zwolnienie (logujemy w pętli, nie w tasku)
  void finish(int code) {
    this->last_status_ = code;
    this->busy_ = false;
  }

 protected:
  std::string build_payload_();

  std::string key_;
  std::string label_;
  float lat_{0.0f};
  float lon_{0.0f};
  bool has_loc_{false};
  bool insecure_{false};           // http zamiast https (omija TLS/cert → mało RAM)
  std::vector<std::pair<sensor::Sensor *, std::string>> sensors_;
  volatile bool busy_{false};      // POST trwa w osobnym tasku → nie startuj kolejnego
  volatile bool pending_{false};   // update() zaznacza chęć wysyłki; start w loop() gdy TLS wolne
  volatile int last_status_{0};    // wynik ostatniego POST (logowany w update())
};

}  // namespace sensmos
}  // namespace esphome
