from flask import Flask, render_template, redirect, url_for, request, jsonify
import paho.mqtt.publish as publish
import requests as http_requests

app = Flask(__name__)

MQTT_BROKER = "localhost"

relay_state = {
    1: False,
    2: False,
    3: False,
    4: False
}

def send_relay(relay, state):
    publish.single(
        topic=f"home/relay/{relay}",
        payload="ON" if state else "OFF",
        hostname=MQTT_BROKER
    )

def update_wifi(ssid, password):
    payload = f'{{"ssid":"{ssid}","password":"{password}"}}'

    publish.single(
        topic="home/wifi/update",
        payload=payload,
        hostname=MQTT_BROKER
    )

@app.route("/")
def index():
    return render_template("index.html", states=relay_state)

@app.route("/toggle/<int:relay>")
def toggle(relay):
    relay_state[relay] = not relay_state[relay]
    send_relay(relay, relay_state[relay])
    return redirect(url_for("index"))

@app.route("/wifi", methods=["POST"])
def wifi():
    ssid = request.form["ssid"]
    password = request.form["password"]

    update_wifi(ssid, password)

    return redirect(url_for("index"))

# WMO weather code → (emoji, description)
WEATHER_CODES = {
    0:  ('☀️',  'Clear Sky'),
    1:  ('🌤️', 'Mainly Clear'),
    2:  ('⛅',  'Partly Cloudy'),
    3:  ('☁️',  'Overcast'),
    45: ('🌫️', 'Foggy'),
    48: ('🌫️', 'Icy Fog'),
    51: ('🌦️', 'Light Drizzle'),
    53: ('🌦️', 'Drizzle'),
    55: ('🌧️', 'Heavy Drizzle'),
    61: ('🌧️', 'Light Rain'),
    63: ('🌧️', 'Rain'),
    65: ('🌧️', 'Heavy Rain'),
    71: ('🌨️', 'Light Snow'),
    73: ('🌨️', 'Snow'),
    75: ('❄️',  'Heavy Snow'),
    80: ('🌦️', 'Rain Showers'),
    81: ('🌧️', 'Heavy Showers'),
    95: ('⛈️',  'Thunderstorm'),
    96: ('⛈️',  'Thunderstorm + Hail'),
    99: ('⛈️',  'Heavy Thunderstorm'),
}

@app.route("/api/weather")
def weather_api():
    try:
        # Step 1: Auto-detect location from public IP
        loc = http_requests.get("http://ip-api.com/json/", timeout=5).json()
        lat  = loc.get("lat", 20.0)
        lon  = loc.get("lon", 78.0)
        city = loc.get("city", "Unknown")

        # Step 2: Fetch weather from Open-Meteo (free, no API key)
        url = (
            f"https://api.open-meteo.com/v1/forecast"
            f"?latitude={lat}&longitude={lon}"
            f"&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
            f"&temperature_unit=celsius&wind_speed_unit=kmh"
        )
        w = http_requests.get(url, timeout=8).json()
        cur = w["current"]

        code  = int(cur.get("weather_code", 0))
        emoji, desc = WEATHER_CODES.get(code, ('🌡️', 'Unknown'))

        return jsonify({
            "city":     city,
            "temp":     round(cur["temperature_2m"]),
            "humidity": cur["relative_humidity_2m"],
            "wind":     round(cur["wind_speed_10m"]),
            "emoji":    emoji,
            "desc":     desc,
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
