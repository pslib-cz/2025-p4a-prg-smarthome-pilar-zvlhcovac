Internet věcí s Home Assistant

Cílem projektu je vytvořit systém automatizace v Home Assistant, který bude simulovat reálné scénáře použití chytré domácnosti nebo komunikační platformy Internetu věcí.

Vytvořte dostatečně reálnou "případovou studii" chytré domácnosti nebo jinou neprůmyslovou implementaci. 

Senzory i akční členy by měly přímo odpovídat charakteru zvolené automatizace nebo scény. Tedy pokud budu sledovat teplotu a vlhkost v prostředí, mohu ovládat ventilátor, AC jednotku, topný člen... i když může jít o malý DC větráček místo dvouzónové AC jednotky.

Veškerá činnost bude centrálně řízena z HA bez závislosti na externím systému (web server, databáze apod.). Na HA bude připojen webový klient, který bude sloužit jako dashboard (monitoring, logování, rekonfigurace). Klient může data čerpat jako MQTT klient (websocket) nebo REST API klient (http). V obou případech by mělo být řešeno zabezpečení (například TLS/SSL).

Scény a na ni navázaná automatizace je předmětem "nápaditosti" realizační skupiny. 
Práce ve skupině

Projekt lze realizovat v individuálně nebo ve skupině studentů. Rozsah činnosti je určen N (počtem členů) teamu:

Minimální počet lokálních integrací: 

pro N < 3 minimum integrací: 1
pro N > 2 minimum integrací: N – 1

Minimální počet scén / scénářů = N

Minimální počet entit = N * 2 (kromě N = 1, kdy je minimum 3)
Hodnocení
Kritérium	Popis	Body
Množství použitých integrací	Počet různých integrací a jejich vhodnost vzhledem k danému scénáři.	10
Množství použitých entit	Celkový počet entit použitých v systému (senzory, akční členy atd.).	10
Čistota/komplexita HW řešení	Správné zapojení a funkčnost hardwaru, minimalizace chyb, udržitelnost návrhu.	10
Nápaditost a reálnost použitých automatizací a scénářů	Jak dobře projekt reflektuje reálné scénáře chytré domácnosti.	20
Unikátnost řešení	Inovativní přístup k řešení problému, originální nápady.	10
Dashboard pro monitoring a logování	Přehlednost, uživatelská přívětivost a funkčnost dashboardu jako nástroje pro sledování dat.	15
Dashboard pro interakci	Možnost uživatelského ovládání systému prostřednictvím dashboardu.	15
Zabezpečení komunikace a autorizace	Použití zabezpečených protokolů (např. TLS/SSL), řízení přístupu.	10

Celkové hodnocení: Maximální počet bodů je 100. Hodnocení bude provedeno na základě funkčnosti, technické správnosti a originality řešení.
Využitelné technologie

Přepokládá se využití zejména vlastních prostředků jak HW, tak SW. 
Doporučené “lokální” integrace / add-ons v HA

    ESPHome Device Builder Getting Started with ESPHome and Home Assistant — ESPHome

    MQTT MQTT - Home Assistant

    Hardwario BCG MarianRychtecky/ha-hardwario: Hardwario USB radio gateway support for Home Assistant s Hardwario Tower moduly

    Local Tuya Hub xZetsubou/hass-localtuya: A Home Assistant integration to handle Tuya devices locally

    Zigbee HA Zigbee Home Automation - Home Assistant

    ZigBee2MQTT zigbee2mqtt/hassio-zigbee2mqtt: Official Zigbee2MQTT Home Assistant add-on (!náročné na HW!)

Další využitelné add-ons

    VS Code Server hassio-addons/addon-vscode: Studio Code Server - Home Assistant Community Add-ons

    Node-RED hassio-addons/addon-node-red: Node-RED - Home Assistant Community Add-ons

    MariaDB addons/mariadb at master · home-assistant/addons

Zajímavé externí integrace (např. pro doplnění entit)

    Sun - Home Assistant

    Generic Camera - Home Assistant

    Sensor.Community - Home Assistant

    Ping (ICMP) - Home Assistant

    Apple HomeKit Bridge - Home Assistant

    Google Assistant SDK - Home Assistant

    Discord - Home Assistant

    Minecraft Server - Home Assistant

    Epic Games Store - Home Assistant

    PrusaLink - Home Assistant

Relevantní HW

Smart Wifi Tuya kompatibilní = nejdostupnější spotřebitelský HW

Vybírejte pouze z “Choice” nabídky - doprava z centrálních skladů do 10–14 dní

Varianty označení ZigBee jsou dražší, protože nekomunikují přes Wifi, ale prostřednictvím IEEE 802.15.4!

 

ESPHome DIY = akční členy i senzory za pár kaček

    centrem je procesor ESP8266 ESP8266 on AliExpress

    nebo ESP32 esp32C3 on AliExpress

    pro moduly (ESP8266) v pouzdře ESP01 jsou dostupná embedded řešení Esp01 Module - AliExpress

    senzorů, které lze přímo připojit jsou desítky sensors on AliExpress

    akční člen může být display, relé, LED regulátor, WS2812B Strip, RFID/NFC, Výkonový stejnosměrný regulátor… 

Shrnutí (non ChatGPT)

    Centrem sběru dat a poskytování informací o senzorech a akčních členech je HA
    Home Assistant https://www.home-assistant.io/
    Snaha o minimalizaci závislosti na "Cloud API" řešeních - co lze, to řešit lokální službou
    Preferovat rozšíření (add-on) nebo integraci v HA, před externí aplikací
    Celá případová studie bude maximálně "kopírovat" realitu – např. pokud budu zapínat klimatizaci, měl bych použít akční člen ventilátor a ne LED diodu

    Podle počtu členů ve skupině bude použit adekvátní počet protokolů, integrací, zařízení (akční členy a senzory)
    Maximalizujte snahu o zabezpečení na jednotlivých úrovních.
    Základní rozhraní: ESPHome, Hardwario BCG, Tyua local, MQTT, Zigbee2MQTT, ZHA
    Využívejte pro vás dostupné zařízení, v omezené míře lze využít vybavení laboratoře / vyučujícího

Zbytečné informace

Při použití procesorů ESP8266/ESP32 je vždy třeba myslet na řešení napájení. Procesory pracují s napětím 2,8V až 3,3V. Tedy přímo lze použít 2ks AA tužkových (alkalických  = nenabíjecích!) baterií, nebo jeden LiFePo4 článek. V ostatních případech je třeba počítat s nutností použit tzv. step-down měnič. A napájet z USB zdroje nebo LiIon/LiPol článku…
Při použití ESP32 je vždy součástí desky USB převodník s 5Vto3V3 stabilizátorem pro programování a tedy je napájení procesoru a maximálně jednoho až dvou senzorů vyřešeno. Pozor! Není možné takto napájet energeticky náročné periferie (motor, vysoce svítivé led, více led diod, LCD display…).