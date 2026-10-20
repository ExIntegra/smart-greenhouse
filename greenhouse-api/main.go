package main

import (
        "encoding/json"
        "log"
        "net/http"
        "os"

        "github.com/prometheus/client_golang/prometheus"
        "github.com/prometheus/client_golang/prometheus/promhttp"
)

type Telemetry struct {
        DeviceID        string   `json:"device_id"`
        SensorID        string   `json:"sensor_id"`
        TemperatureC    *float64 `json:"temperature_c"`
        HumidityPercent *float64 `json:"humidity_percent"`
        LightPercent    *float64 `json:"light_percent"`
}


var light = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
                Name: "greenhouse_light_percent",
                Help: "Current greenhouse light",
        },
        []string{"device_id", "sensor_id"},
)
var temperature = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
                Name: "greenhouse_temperature_celsius",
                Help: "Current greenhouse temperature",
        },
        []string{"device_id", "sensor_id"},
)

var humidity = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
                Name: "greenhouse_humidity_percent",
                Help: "Current greenhouse humidity",
        },
        []string{"device_id", "sensor_id"},
)

var telemetryTotal = prometheus.NewCounterVec(
        prometheus.CounterOpts{
                Name: "greenhouse_telemetry_received_total",
                Help: "Total telemetry messages received",
        },
        []string{"device_id"},
)

func telemetryHandler(w http.ResponseWriter, r *http.Request) {
        if r.Method != http.MethodPost {
                http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
                return
        }

        apiKey := os.Getenv("API_KEY")

        if r.Header.Get("X-API-Key") != apiKey {
                http.Error(w, "unauthorized", http.StatusUnauthorized)
                return
        }

        var data Telemetry

        decoder := json.NewDecoder(r.Body)
        err := decoder.Decode(&data)

        if err != nil {
                http.Error(w, "invalid json", http.StatusBadRequest)
                return
        }

        if data.DeviceID == "" || data.SensorID == "" {
                http.Error(w, "device_id and sensor_id are required", http.StatusBadRequest)
                return
        }

        if data.TemperatureC != nil {
                if *data.TemperatureC < -55 || *data.TemperatureC > 125 {
                        http.Error(w, "invalid temperature", http.StatusBadRequest)
                        return
                }
                temperature.WithLabelValues(
                        data.DeviceID,
                        data.SensorID,
                ).Set(*data.TemperatureC)
        }

        if data.HumidityPercent != nil {
                if *data.HumidityPercent < 0 || *data.HumidityPercent > 100 {
                        http.Error(w, "invalid humidity", http.StatusBadRequest)
                        return
                }
                humidity.WithLabelValues(
                        data.DeviceID,
                        data.SensorID,
                ).Set(*data.HumidityPercent)
        }

        if data.LightPercent != nil {
                if *data.LightPercent < 0 || *data.LightPercent > 100 {
                        http.Error(w, "invalid light", http.StatusBadRequest)
                        return
                }
                light.WithLabelValues(
                        data.DeviceID,
                        data.SensorID,
                ).Set(*data.LightPercent)
        }

        telemetryTotal.WithLabelValues(
                data.DeviceID,
        ).Inc()

        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(map[string]string{
                "status": "ok",
        })
}

func healthHandler(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Content-Type", "application/json")

        json.NewEncoder(w).Encode(map[string]string{
                "status": "ok",
        })
}

func main() {
        if os.Getenv("API_KEY") == "" {
                log.Fatal("API_KEY environment variable is not set")
        }

        prometheus.MustRegister(
                temperature,
                telemetryTotal,
                light,
                humidity,
        )

        http.HandleFunc("/api/v1/telemetry", telemetryHandler)
        http.HandleFunc("/health", healthHandler)
        http.Handle("/metrics", promhttp.Handler())

        log.Println("greenhouse-api listening on :8000")

        if err := http.ListenAndServe(":8000", nil); err != nil {
                log.Fatal(err)
        }
}
