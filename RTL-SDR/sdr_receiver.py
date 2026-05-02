import time
import json
import numpy as np
from rtlsdr import RtlSdr
import paho.mqtt.client as mqtt

MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "SDR_CLIENT"

TOPIC_POWER = "project/sdr/power_db"
TOPIC_STATUS = "project/sdr/signal_detected"
TOPIC_FREQ = "project/sdr/center_freq_hz"
TOPIC_JSON = "project/sdr/telemetry"

TOPIC_STRONGEST_FREQ = "project/sdr/strongest_freq_hz"
TOPIC_STRONGEST_POWER = "project/sdr/strongest_power_db"
TOPIC_STATION_DETECTED = "project/sdr/station_detected"
TOPIC_TOP3 = "project/sdr/top3"
TOPIC_ALERT = "project/sdr/alert"
TOPIC_SPECTRUM = "project/sdr/spectrum"

START_FREQ = 88e6
END_FREQ = 108e6
STEP_FREQ = 0.5e6
SAMPLE_RATE = 1.024e6
GAIN = "auto"
NUM_SAMPLES = 64 * 1024
SPECTRUM_POINTS = 256
POWER_THRESHOLD_DB = -10.5
DWELL_TIME_SEC = 0.6

def iq_to_avg_power_db(samples: np.ndarray) -> float:
    power_linear = np.mean(np.abs(samples) ** 2)
    power_db = 10 * np.log10(power_linear + 1e-12)
    return float(power_db)

def get_spectrum(samples: np.ndarray, sample_rate: float, spectrum_points: int):
    fft_vals = np.fft.fftshift(np.fft.fft(samples))
    spectrum_power = 20 * np.log10(np.abs(fft_vals) + 1e-12)

    noise_floor = np.percentile(spectrum_power, 5)
    spectrum_power = spectrum_power - noise_floor
    spectrum_power = np.clip(spectrum_power, 0, 80)

    freqs = np.linspace(-sample_rate / 2, sample_rate / 2, len(spectrum_power), endpoint=False)

    step = max(1, len(freqs) // spectrum_points)

    spectrum = [
        {
            "freq_offset_hz": float(f),
            "power_db": float(p)
        }
        for f, p in zip(freqs[::step], spectrum_power[::step])
    ]

    return spectrum

def setup_mqtt():
    client = mqtt.Client(client_id=MQTT_CLIENT_ID, protocol=mqtt.MQTTv311)
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    return client

def setup_sdr():
    sdr = RtlSdr()
    sdr.sample_rate = SAMPLE_RATE
    sdr.gain = GAIN
    return sdr

def build_freq_list(start_hz, end_hz, step_hz):
    freqs = []
    f = start_hz
    while f <= end_hz:
        freqs.append(f)
        f += step_hz
    return freqs

def publish_current(client, freq, power_db, detected, spectrum_data):
    payload = {
        "center_freq_hz": freq,
        "sample_rate_hz": SAMPLE_RATE,
        "power_db": round(power_db, 2),
        "signal_detected": detected,
        "timestamp": time.time(),
        "spectrum": spectrum_data
    }

    client.publish(TOPIC_POWER, f"{power_db:.2f}", retain=True)
    client.publish(TOPIC_STATUS, "1" if detected else "0", retain=True)
    client.publish(TOPIC_FREQ, str(int(freq)), retain=True)
    client.publish(TOPIC_JSON, json.dumps(payload), retain=True)
    client.publish(TOPIC_SPECTRUM, json.dumps(payload), retain=True)

def publish_summary(client, strongest_freq, strongest_power, top3, station_detected, alert_msg="System Normal"):
    client.publish(TOPIC_STRONGEST_FREQ, str(int(strongest_freq)), retain=True)
    client.publish(TOPIC_STRONGEST_POWER, f"{strongest_power:.2f}", retain=True)
    client.publish(TOPIC_STATION_DETECTED, "1" if station_detected else "0", retain=True)
    client.publish(TOPIC_TOP3, json.dumps(top3), retain=True)
    client.publish(TOPIC_ALERT, alert_msg, retain=True)

def main():
    client = setup_mqtt()
    sdr = setup_sdr()

    freq_list = build_freq_list(START_FREQ, END_FREQ, STEP_FREQ)

    print("Smart FM scanning started")
    print(f"Scanning from {int(START_FREQ)} Hz to {int(END_FREQ)} Hz")
    print(f"Step: {STEP_FREQ / 1e6} MHz")
    print(f"Threshold: {POWER_THRESHOLD_DB} dB")
    print(f"Spectrum points: {SPECTRUM_POINTS}")

    last_strongest_freq = None

    try:
        while True:
            scan_results = []

            for freq in freq_list:
                try:
                    sdr.center_freq = freq
                    time.sleep(0.15)

                    samples = sdr.read_samples(NUM_SAMPLES)
                    power_db = iq_to_avg_power_db(samples)
                    detected = power_db > POWER_THRESHOLD_DB

                    spectrum_data = get_spectrum(samples, SAMPLE_RATE, SPECTRUM_POINTS)

                    publish_current(client, freq, power_db, detected, spectrum_data)

                    scan_results.append({
                        "freq_hz": int(freq),
                        "power_db": round(power_db, 2),
                        "signal_detected": detected
                    })

                    print(scan_results[-1])
                    time.sleep(DWELL_TIME_SEC)

                except Exception as read_error:
                    print(f"Read error at {int(freq)} Hz: {read_error}")
                    try:
                        sdr.close()
                    except Exception:
                        pass
                    time.sleep(2)
                    try:
                        sdr = setup_sdr()
                        print("SDR reinitialized successfully.")
                    except Exception as init_error:
                        print(f"Failed to reinitialize SDR: {init_error}")
                        time.sleep(3)

            if not scan_results:
                continue

            sorted_results = sorted(scan_results, key=lambda x: x["power_db"], reverse=True)

            strongest = sorted_results[0]
            strongest_freq = strongest["freq_hz"]
            strongest_power = strongest["power_db"]
            station_detected = strongest_power > POWER_THRESHOLD_DB

            top3 = sorted_results[:3]

            alert_msg = "System Normal"

            if last_strongest_freq is not None and strongest_freq != last_strongest_freq:
                alert_msg = f"Station changed → {last_strongest_freq/1e6:.1f} MHz → {strongest_freq/1e6:.1f} MHz"

            if strongest_power > -7:
                alert_msg = f"Strong signal at {strongest_freq/1e6:.1f} MHz ({strongest_power:.2f} dB)"

            last_strongest_freq = strongest_freq

            publish_summary(
                client,
                strongest_freq,
                strongest_power,
                top3,
                station_detected,
                alert_msg
            )

            print({
                "strongest_freq_hz": strongest_freq,
                "strongest_power_db": strongest_power,
                "station_detected": station_detected,
                "top3": top3,
                "alert": alert_msg
            })

    except KeyboardInterrupt:
        print("Stopping...")

    finally:
        try:
            sdr.close()
        except Exception:
            pass
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
