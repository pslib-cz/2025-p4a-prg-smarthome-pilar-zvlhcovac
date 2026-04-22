/*
 * secrets.h — TAJNÉ PŘIHLAŠOVACÍ ÚDAJE
 * ======================================
 * Zkopíruj tento soubor jako secrets.h a vyplň své hodnoty.
 * NIKDY nenahrávej secrets.h na GitHub — přidej ho do .gitignore!
 *
 * Postup generování MQTT CA certifikátu na Raspberry Pi:
 *   openssl req -new -x509 -days 1826 -extensions v3_ca \
 *     -keyout ca.key -out ca.crt
 *   Obsah ca.crt pak vlož jako MQTT_CA_CERT níže.
 */

#pragma once

// ---- WiFi ----
#define WIFI_SSID     "joud"
#define WIFI_PASSWORD "123456789"

// ---- MQTT Broker (Mosquitto na Raspberry Pi) ----
#define MQTT_BROKER   "10.45.182.185"  // IP adresa Raspberry Pi
#define MQTT_PORT     8883             // TLS port
#define MQTT_USER     "humidifier"
#define MQTT_PASS     "Heslo123"

// ---- CA certifikát Mosquitto brokeru (PEM formát) ----
// Celý obsah ca.crt souboru jako C string:
#define MQTT_CA_CERT \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDXzCCAkegAwIBAgIUeIpFUfB0y+zfGiphIFlnwcCwo04wDQYJKoZIhvcNAQEL\n" \
"BQAwPzELMAkGA1UEBhMCQ1oxEjAQBgNVBAoMCVNtYXJ0SG9tZTEcMBoGA1UEAwwT\n" \
"aG9tZWFzc2lzdGFudC5sb2NhbDAeFw0yNjA0MjIyMDE1NDhaFw0zMTA0MjIyMDE1\n" \
"NDhaMD8xCzAJBgNVBAYTAkNaMRIwEAYDVQQKDAlTbWFydEhvbWUxHDAaBgNVBAMM\n" \
"E2hvbWVhc3Npc3RhbnQubG9jYWwwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQCYkyGAntvA4FP42khxYCcJ6WXa0fS1hewa4Um9fUnwuNPr2w+k0W7xhFC3\n" \
"qMk4kZPZqUFrRNqgZdF1ai3GJ5cdtk199x5Ni6MywAJoCkcnDDg7WnBY7vN3gGMB\n" \
"DwGJ2ueB9/khGzF6Ck5emCbVn2DSRmO1pkLx/1ICyg5qTa67Ys3UCgBrCRNjKFSx\n" \
"oUzl9nQCYGNxVpKIYBFDfYqA5I0yP54nWi2vuLR/JVTh2Ru0DEaOeEfvlA6blg4d\n" \
"WN0h4pFuWfHgeTh9uuR6KidTGileEfaV1QCSEsRuPXQgP/pXZNE4xph2+mfBOUjJ\n" \
"q94rkkUqWAslanQGEMJfJD1n2/KnAgMBAAGjUzBRMB0GA1UdDgQWBBR0lYVq7iv4\n" \
"rSwiUs7uOOMMMqAE/jAfBgNVHSMEGDAWgBR0lYVq7iv4rSwiUs7uOOMMMqAE/jAP\n" \
"BgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBO9Dkm/bUWqCM9vE+L\n" \
"5sshbctIhCEvQXJ0M1QL+NlG54A7r9770MBNPax9IYlzaF+oQkbc+Hz6aShIjf2h\n" \
"sGlKZ0fdrrNGEzA4oa8od43BWOo1Ve+Lqo3CNenxDhIIVtbWjVvqpJccujkzUx+6\n" \
"p12/CxadMAgopQD5pZVusn1N7snsCOK1jw4uUOskeYwQEt8OAHTOSSf42hSj64qN\n" \
"cK1VAp7BmeWEFg/TIXIVSLH3Ou1/Jc/XNJU0FgZCHSQYB5FOFpjtAiEve8n7NjP2\n" \
"r/mt3tyW4+2kvZRky8nGox2KERjxup71jKKnmsbSYJqDh+w1sDy9DYemsj4SSvaH\n" \
"xVYY\n" \
"-----END CERTIFICATE-----\n"

