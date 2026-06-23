#include "sensmos.h"
#include "sensmos_net.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <cmath>
#include <cstdio>
#include <new>

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
static const char *const INGEST_PATH = "api.sensmos.com/v1/ingest";  // bez schematu (http/https z flagi)

// Blokujący POST — uruchamiany w OSOBNYM tasku (nie w pętli), żeby HTTPS nie dławił
// głównej pętli/BLE. Bez logowania tutaj — logger ESPHome nie jest thread-safe; wynik
// wraca przez finish() i jest logowany w update(). URL http:// → bez TLS (mało RAM).
static int sensmos_http_post(const std::string &url, const std::string &body) {
#ifdef USE_ESP_IDF
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.method = HTTP_METHOD_POST;
  cfg.timeout_ms = 8000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;  // używane tylko dla https; dla http ignorowane
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body.c_str(), body.size());
  esp_err_t err = esp_http_client_perform(client);
  int code = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;
  esp_http_client_cleanup(client);
  return code;
#elif defined(USE_ARDUINO)
  HTTPClient http;
  WiFiClientSecure sclient;
  bool started;
  if (url.rfind("https://", 0) == 0) {
    sclient.setInsecure();  // v1: bez weryfikacji certu (telemetria, niski risk)
    started = http.begin(sclient, url.c_str());
  } else {
    started = http.begin(url.c_str());  // plain http — bez TLS
  }
  if (!started)
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

// wykonywane w trwałym workerze sensmos_net: HTTP POST + zapis wyniku (logujemy w pętli, nie tu)
static void post_run(SensmosJob *job) {
  int code = sensmos_http_post(job->url, job->body);
  static_cast<SensmosComponent *>(job->self)->finish(code);
}

void SensmosComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Sensmos:");
  ESP_LOGCONFIG(TAG, "  Endpoint: %s://%s", this->insecure_ ? "http" : "https", INGEST_PATH);
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
  this->pending_ = true;  // chcemy wysłać; faktyczny start w loop() gdy łącze TLS wolne
}

void SensmosComponent::loop() {
  if (!this->pending_ || this->busy_)
    return;
  if (!network::is_connected())
    return;

  std::string body = this->build_payload_();
  std::string url = (this->insecure_ ? "http://" : "https://") + std::string(INGEST_PATH);
  // nothrow: przy braku RAM ZWRÓĆ null zamiast crashować node (wyjątki w ESP-IDF wyłączone)
  auto *job = new (std::nothrow) SensmosJob{post_run, this, std::move(url), std::move(body)};
  if (job == nullptr) {
    ESP_LOGW(TAG, "Low heap — skipping push");
    return;  // pending_ zostaje → spróbuje w kolejnym cyklu
  }
  this->pending_ = false;
  this->busy_ = true;
  ESP_LOGD(TAG, "Pushing %d entities to map", (int) this->sensors_.size());
  // do trwałego workera (statyczny stos) — bez tworzenia taska per request → bez fragmentacji
  if (!net_submit(job)) {
    this->busy_ = false;
    delete job;
  }
}

}  // namespace sensmos
}  // namespace esphome
