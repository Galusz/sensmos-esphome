#include "get_sensor.h"
#include "sensmos_net.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/components/json/json_util.h"
#include <cstdlib>
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

static const char *const TAG = "sensmos.get";
static const char *const GET_PATH = "api.sensmos.com/v1/ingest/get/";  // bez schematu (http/https z flagi)
static const size_t MAX_BODY = 4096;  // odpowiedzi są małe; mniejszy bufor = mniej sterty

// Blokujący GET ciała odpowiedzi — uruchamiany w OSOBNYM tasku (nie w pętli). Bez logowania.
static bool sensmos_http_get(const std::string &url, std::string &out) {
  out.clear();
#ifdef USE_ESP_IDF
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.method = HTTP_METHOD_GET;
  cfg.timeout_ms = 8000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (esp_http_client_open(client, 0) != ESP_OK) {
    esp_http_client_cleanup(client);
    return false;
  }
  esp_http_client_fetch_headers(client);
  char buf[512];
  int r;
  while ((r = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
    out.append(buf, r);
    if (out.size() > MAX_BODY) break;
  }
  int status = esp_http_client_get_status_code(client);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return status == 200 && !out.empty();
#elif defined(USE_ARDUINO)
  HTTPClient http;
  WiFiClientSecure sclient;
  bool started;
  if (url.rfind("https://", 0) == 0) {
    sclient.setInsecure();  // v1: bez weryfikacji certu (dane publiczne, niski risk)
    started = http.begin(sclient, url.c_str());
  } else {
    started = http.begin(url.c_str());  // plain http — bez TLS
  }
  if (!started)
    return false;
  http.setTimeout(8000);
  int code = http.GET();
  if (code == 200)
    out = std::string(http.getString().c_str());
  http.end();
  return code == 200 && !out.empty();
#else
  return false;
#endif
}

// Wyciąga value dla danej encji z { "entities":[{"entity_id":..,"value":..}] }.
static bool parse_value(const std::string &body, const std::string &entity, float &out) {
  bool found = false;
  json::parse_json(body, [&](JsonObject root) -> bool {
    if (!root["entities"].is<JsonArray>())
      return false;
    for (JsonVariant item : root["entities"].as<JsonArray>()) {
      JsonObject e = item.as<JsonObject>();
      const char *eid = e["entity_id"] | "";
      if (entity == eid) {
        JsonVariant v = e["value"];
        if (v.is<const char *>())
          out = atof(v.as<const char *>());
        else
          out = v.as<float>();
        found = true;
        return true;
      }
    }
    return true;
  });
  return found;
}

// wykonywane w trwałym workerze sensmos_net: HTTP GET + zapis odpowiedzi (parsowanie w pętli)
static void get_run(SensmosJob *job) {
  std::string resp;
  bool ok = sensmos_http_get(job->url, resp);
  static_cast<SensmosGetSensor *>(job->self)->finish_body(resp, ok);
}

void SensmosGetSensor::dump_config() {
  LOG_SENSOR("", "Sensmos Get", this);
  ESP_LOGCONFIG(TAG, "  Node: %s", this->device_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Entity: %s", this->entity_.c_str());
}

void SensmosGetSensor::update() {
  this->pending_ = true;  // chcemy pobrać; faktyczny start w loop() gdy łącze TLS wolne
}

void SensmosGetSensor::loop() {
  // 1) skonsumuj wynik poprzedniego pobrania (parsowanie + publish w pętli — logger thread-safe)
  if (this->have_body_) {
    this->have_body_ = false;
    if (this->http_ok_) {
      float val;
      if (parse_value(this->body_, this->entity_, val))
        this->publish_state(val);
      else
        ESP_LOGW(TAG, "Entity %s not found for node %s", this->entity_.c_str(),
                 this->device_id_.c_str());
    } else {
      ESP_LOGW(TAG, "GET failed for node %s", this->device_id_.c_str());
    }
    this->body_.clear();
    this->body_.shrink_to_fit();
  }

  // 2) wystartuj nowe pobranie, gdy zaplanowane i łącze TLS wolne (jedno naraz w całym komponencie).
  // Jak zajęte, próbujemy w kolejnym loop() — bez czekania na następny interwał (brak głodzenia).
  if (this->pending_ && !this->busy_ && network::is_connected()) {
    std::string url = (this->insecure_ ? "http://" : "https://") + std::string(GET_PATH) + this->device_id_;
    // nothrow: przy braku RAM ZWRÓĆ null zamiast crashować node (wyjątki w ESP-IDF wyłączone)
    auto *job = new (std::nothrow) SensmosJob{get_run, this, std::move(url), std::string()};
    if (job == nullptr) {
      ESP_LOGW(TAG, "Low heap — skipping fetch");
      return;  // pending_ zostaje → spróbuje w kolejnym cyklu
    }
    this->pending_ = false;
    this->busy_ = true;
    // do trwałego workera (statyczny stos) — bez tworzenia taska per request → bez fragmentacji
    if (!net_submit(job)) {
      this->busy_ = false;
      delete job;
    }
  }
}

}  // namespace sensmos
}  // namespace esphome
