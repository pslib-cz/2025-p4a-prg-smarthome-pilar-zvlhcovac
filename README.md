# Smart Home — Automatický Zvlhčovač Vzduchu

Případová studie chytré domácnosti. Tento projekt kombinuje autonomní mikrokontrolér s nadřazeným řízením přes Home Assistant a nabízí jak lokální ovládání (díky integrovanému dotykovému displeji), tak vzdálený monitoring.

## 📋 Specifikace projektu
- **Tým:** Individuální projekt (Ondřej)
- **Minimální požadavky (tým N=1):**
  - ≥ 1 integrace (zde použity 2: MQTT a Sun / automatizace času)
  - ≥ 1 scéna/scénář (zde použity 3: Standalone regulace, Noční režim, Offline ochrana)
  - ≥ 3 entity (zde použito 8 entit přes MQTT Discovery)

## 🛠 Hardwarová architektura
- **Mikrokontrolér:** ESP32-2432S028 (CYD) s 2.8" SPI TFT a VSPI Touchscreen
- **Senzor:** SHT30 (Teplota a Vlhkost) připojený přes I²C (SDA=22, SCL=27)
- **Aktuátor:** Zvlhčovací modul STDZ-1810 spínaný přes I²C Expander PCF8574 (bit 0)
- **Server:** Raspberry Pi s Home Assistant OS a Mosquitto MQTT Brokerem (TLS 8883 / WSS 9001)

## ⚙️ Softwarová architektura a Scénáře

Zařízení primárně využívá koncept **"Standalone First"**.

**1. Standalone autonomní režim (Základní fungování):**
Mikrokontrolér udržuje vlhkost pomocí ±2 % hysteréze zcela nezávisle na nadřazených systémech. Při ztrátě WiFi spojení se tedy systém nezastaví, ale pokračuje v regulaci a v případě poškození senzoru (SHT30 nelze vyčíst) aktivuje integrovaný fallback záchranný režim (zvlhčovač se zcela vypne, aby nedošlo ke kondenzaci). Na displeji lze zároveň ovládat cílovou vlhkost i aktuální stav modulu ručně.

**2. Noční režim (Home Assistant a automatizace):**
V HA je konfigurována automatizace času, která v 22:00 zašle skrz rozhraní MQTT příkaz pro úpravu definované cílové vlhkosti (zvýšení na doporučených 55 % v průběhu noci). Automatizace tuto hodnotu následně vrátí k normálu na 50 % za úsvitu v 07:00.

**3. Offline ochranný monitoring (Web Dashboard & HA):**
Dříve zmíněný fallback je hlášen do nadřazeného systému. HA upozorní ve vlastním uživatelském prostředí chybovou notifikací o nečekané ztrátě spojení k zařízení prostřednictvím the Last Will & Testament packetu poslaného od Mosquitto brokeru. Rovněž tak webový interaktivní dashboard (v HTML & JS za využití portu WSS 9001) označí zařízení patřičným způsobem a nedovolí uživateli odesílat požadavky pro úpravu, dokud spojení nedojde úspěšné nápravy.

## 🔐 Zabezpečení
Projektní stack respektuje základní bezpečnostní standardy od doporučení od CIoT:
1. **IoT Síť:** Zařízení se připojuje přes dedikovanou WPA3 síť, veškeré přihlašovací údaje k sítit tak leží mimo soubory verze git – pod explicitním `secrets.h`.
2. **Kryptografická ochrana relace:** MQTT transport probíhá striktně skrze TLS protokol (Port 8883) spojením a ověřením certifikátu pomocí třídy `WiFiClientSecure`. Integrace MQTT podporuje certifikát na úrovni Root CA Mosquitta.
3. **Přístupová práva UI vrstvy:**  Webová integrace probíhá přes WSS (WebSockets přes TLS na Portu 9001), zaručující bezproblémové propojené z jakéhokoliv standardního klienta a pro přístup k metrice a interakcím navazuje požadavek na zadané pověření.

## 📦 Struktura projektu
- `/humidifier` – Kompletní Arduino kód založený na sadách starého kódu (původní `old_project_sketch.ino`). Podpora pro TFT_eSPI, LVGL, I²C.
- `/homeassistant` – YAML konfigurace definující lovelace panely a rutinní logiku (night_mode & alert routines).
- `/dashboard` – Webklient HTML/CSS/JS pro real - time grafy z telemetrie zařízení skrz MQTT Over WSS.
