#include "sensmos.h"
#include "sensmos_net.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <cmath>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// Blokujący POST — uruchamiany w OSOBNYM tasku (nie w pętli), żeby HTTPS nie dławił
// głównej pętli/BLE. Bez logowania tutaj — logger ESPHome nie jest thread-safe; wynik
// wraca przez finish() i jest logowany w update().
static int sensmos_http_post(const std::string &body) {
#ifdef USE_ESP_IDF
  esp_http_client_config_t cfg = {};
  cfg.url = INGEST_URL;
  cfg.method = HTTP_METHOD_POST;
  cfg.timeout_ms = 8000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body.c_str(), body.size());
  esp_err_t err = esp_http_client_perform(client);
  int code = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;
  esp_http_client_cleanup(client);
  return code;
#elif defined(USE_ARDUINO)
  WiFiClientSecure client;
  client.setInsecure();  // v1: bez weryfikacji certu (dane telemetryczne, niski risk)
  HTTPClient http;
  if (!http.begin(client, INGEST_URL))
    return -1;
  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t *) body.c_str(), body.size());
  http.end();
  return code;
#else
  return -1;
#endif
}

struct PostJob {
  SensmosComponent *self;
  std::string body;
};

static void sensmos_post_task(void *arg) {
  PostJob *job = static_cast<PostJob *>(arg);
  int code = sensmos_http_post(job->body);
  job->self->finish(code);
  net_release();  // zwolnij łącze TLS dla innych komponentów
  delete job;
  vTaskDelete(nullptr);
}

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
    const std::string &unit = s->get_unit_of_measurement_ref();
    if (!unit.empty())
      j += ",\"unit\":\"" + unit + "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

void SensmosComponent::update() {
  // wynik poprzedniej wysyłki (z taska) — logujemy tu, w pętli
  if (this->last_status_ != 0) {
    ESP_LOGD(TAG, "Ingest HTTP %d", this->last_status_);
    this->last_status_ = 0;
  }
  if (!network::is_connected()) {
    ESP_LOGW(TAG, "No network — skipping push");
    return;
  }
  if (this->busy_) {
    ESP_LOGW(TAG, "Previous push still running — skipping this cycle");
    return;
  }
  // tylko jedno połączenie TLS naraz (publish + readbacki) — inaczej alloc fail na ESP32+BLE
  if (!net_acquire()) {
    ESP_LOGD(TAG, "TLS busy (other Sensmos request) — skipping this cycle");
    return;
  }

  std::string body = this->build_payload_();
  ESP_LOGD(TAG, "Pushing %d entities to map", (int) this->sensors_.size());

  this->busy_ = true;
  auto *job = new PostJob{this, std::move(body)};
  // osobny task: blokujący HTTPS/TLS nie zatrzymuje pętli ESPHome ani BLE
  if (xTaskCreate(sensmos_post_task, "sensmos_post", 10240, job,
                  tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    ESP_LOGW(TAG, "Failed to start push task");
    this->busy_ = false;
    net_release();
    delete job;
  }
}

}  // namespace sensmos
}  // namespace esphome
