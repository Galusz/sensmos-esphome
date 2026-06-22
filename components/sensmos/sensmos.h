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
  void add_sensor(sensor::Sensor *s, const std::string &entity) {
    this->sensors_.emplace_back(s, entity);
  }

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  std::string build_payload_();
  void post_(const std::string &body);

  std::string key_;
  std::string label_;
  float lat_{0.0f};
  float lon_{0.0f};
  bool has_loc_{false};
  std::vector<std::pair<sensor::Sensor *, std::string>> sensors_;
};

}  // namespace sensmos
}  // namespace esphome
