#include "sensmos.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <cmath>
#include <cstdio>

#ifdef USE_ESP_IDF
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#endif
#ifdef USE_ARDUINO
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

namespace esphome {
namespace sensmos {

static const char *const TAG = "sensmos";
static const char *const INGEST_URL = "https://api.sensmos.com/v1/ingest";

void SensmosComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Sensmos:");
  ESP_LOGCONFIG(TAG, "  Endpoint: %s", INGEST_URL);
  ESP_LOGCONFIG(TAG, "  Mapped entities: %d", (int) this->sensors_.size());
  if (this->has_loc_)
    ESP_LOGCONFIG(TAG, "  Location: %.5f, %.5f", this->lat_, this->lon_);
  else
    ESP_LOGCONFIG(TAG, "  Location: from GeoIP");
}

std::string SensmosComponent::build_payload_() {
  std::string j = "{\"key\":\"" + this->key_ + "\"";
  if (this->has_loc_) {
    char buf[64];
    snprintf(buf, sizeof(buf), ",\"lat\":%.6f,\"lon\":%.6f", this->lat_, this->lon_);
    j += buf;
  }
  if (!this->label_.empty())
    j += ",\"label\":\"" + this->label_ + "\"";

  j += ",\"entities\":[";
  bool first = true;
  for (auto &p : this->sensors_) {
    sensor::Sensor *s = p.first;
    if (s == nullptr || !s->has_state() || std::isnan(s->state))
      continue;
    if (!first)
      j += ",";
    first = false;
    char val[32];
    snprintf(val, sizeof(val), "%.4f", s->state);
    j += "{\"entity_id\":\"" + p.second + "\",\"value\":\"" + val + "\"";
    std::string unit = s->get_unit_of_measurement();
    if (!unit.empty())
      j += ",\"unit\":\"" + unit + "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

void SensmosComponent::update() {
  if (!network::is_connected()) {
    ESP_LOGW(TAG, "No network — skipping push");
    return;
  }
  std::string body = this->build_payload_();
  ESP_LOGD(TAG, "Pushing %d entities to map", (int) this->sensors_.size());
  this->post_(body);
}

void SensmosComponent::post_(const std::string &body) {
#ifdef USE_ESP_IDF
  esp_http_client_config_t cfg = {};
  cfg.url = INGEST_URL;
  cfg.method = HTTP_METHOD_POST;
  cfg.timeout_ms = 6000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body.c_str(), body.size());
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    ESP_LOGD(TAG, "Ingest HTTP %d", esp_http_client_get_status_code(client));
  } else {
    ESP_LOGW(TAG, "Ingest failed: %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
#elif defined(USE_ARDUINO)
  WiFiClientSecure client;
  client.setInsecure();  // v1: bez weryfikacji certu (dane telemetryczne, niski risk)
  HTTPClient http;
  if (!http.begin(client, INGEST_URL)) {
    ESP_LOGW(TAG, "http.begin failed");
    return;
  }
  http.setTimeout(6000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t *) body.c_str(), body.size());
  ESP_LOGD(TAG, "Ingest HTTP %d", code);
  http.end();
#endif
}

}  // namespace sensmos
}  // namespace esphome
