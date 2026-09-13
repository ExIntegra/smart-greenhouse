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
	DeviceID     string  `json:"device_id"`
	SensorID     string  `json:"sensor_id"`
	TemperatureC float64 `json:"temperature_c"`
}

var temperature = prometheus.NewGaugeVec(
	prometheus.GaugeOpts{
		Name: "greenhouse_temperature_celsius",
		Help: "Current greenhouse temperature",
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

	if err := json.NewDecoder(r.Body).Decode(&data); err != nil {
		http.Error(w, "invalid json", http.StatusBadRequest)
		return
	}

	if data.DeviceID == "" || data.SensorID == "" {
		http.Error(w, "device_id and sensor_id are required", http.StatusBadRequest)
		return
	}

	if data.TemperatureC < -55 || data.TemperatureC > 125 {
		http.Error(w, "invalid temperature", http.StatusBadRequest)
		return
	}

	temperature.WithLabelValues(
		data.DeviceID,
		data.SensorID,
	).Set(data.TemperatureC)

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
	)

	http.HandleFunc("/api/v1/telemetry", telemetryHandler)
	http.HandleFunc("/health", healthHandler)
	http.Handle("/metrics", promhttp.Handler())

	log.Println("greenhouse-api listening on :8000")

	if err := http.ListenAndServe(":8000", nil); err != nil {
		log.Fatal(err)
	}
}
