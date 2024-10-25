#include <ESP8266WiFi.h> //Библиотека для работы с WIFI
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> // Библиотека для OTA-прошивки
#include <PubSubClient.h>
#include <GyverTimer.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <ErriezDHT22.h>
DHT22 dht22 = DHT22(D1);
WiFiClient espClient;
PubSubClient client(espClient);
GTimer_ms dht_t;
GTimer_ms OTA_Wifi;
GTimer_ms set_manual;
GTimer_ms provetrivanie;
const char *ssid = "Beeline";                        // Имя точки доступа WIFI
const char *name_client = "masterskaja-ventilacija"; // Имя клиента и сетевого порта для ОТА
const char *password = "sl908908908908sl";           // пароль точки доступа WIFI
const char *mqtt_server = "192.168.1.221";
const char *mqtt_reset = "masterskaja-ventilacija_reset"; // Имя топика для перезагрузки
String s;
float temperatura_set = 22.8;
float temperatura;
int flag_pub = 1;
byte state = 0, state_mem = 10, manual = 0, taimer = 0;
float hum_raw, temp_raw, temp_sr = 0;
int data, dht_tik = 0;
int time_g = 0;
int graf = 1;
float temper_ulica = 6;

void callback(char *topic, byte *payload, unsigned int length) // Функция Приема сообщений
{
  String s = ""; // очищаем перед получением новых данных
  for (unsigned int i = 0; i < length; i++)
  {
    s = s + ((char)payload[i]); // переводим данные в String
  }

  if ((String(topic)) == "Clock")
  {
    time_g = atoi(s.c_str()); // переводим данные в int
    if (time_g < 21600 || time_g > 82800)
    {
      temperatura = temperatura_set - 3;
    }
    else
    {
      temperatura = temperatura_set;
    }
  }
  else if ((String(topic)) == "temp_zapad")
  {
    temper_ulica = atof(s.c_str()); // переводим данные в float
  }
  /*
  else if ((String(topic)) == "masterskaja_ven_manual")
  {
    // state = atof(s.c_str()); // переводим данные в float
    // set_manual.reset();
    // set_manual.start();
    //  manual = 1;
  }

  else if ((String(topic)) == name_client && data == 1)
  {
    digitalWrite(D7, HIGH);
    state = 1;
  }
  else if ((String(topic)) == name_client && data == 0)
  {
    digitalWrite(D7, LOW);
    state = 0;
  }
*/
}

void wi_fi_con()
{
  WiFi.mode(WIFI_STA);
  WiFi.hostname(name_client); // Имя клиента в сети
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(name_client); // Задаем имя сетевого порта
  ArduinoOTA.begin();                  // Инициализируем OTA
}

void publish_send(const char *top, float &ex_data) // Отправка Показаний с сенсоров
{
  char send_mqtt[10];
  dtostrf(ex_data, -2, 1, send_mqtt);
  client.publish(top, send_mqtt, 1);
}

void loop()
{

  if (state != state_mem)
  {
    state_mem = state;

    if (state)
    {
      client.publish(name_client, "1");
      digitalWrite(D7, HIGH);
    }
    else
    {
      client.publish(name_client, "0");
      digitalWrite(D7, LOW);
    }
  }

  if (set_manual.isReady())
  {
    manual = 0;
  }

  ESP.wdtFeed();

  if (OTA_Wifi.isReady()) // Поддержание "WiFi" и "OTA"  и Пинок :) "watchdog" и подписка на "Топики Mqtt"
  {
    ArduinoOTA.handle();     // Всегда готовы к прошивке
    client.loop();           // Проверяем сообщения и поддерживаем соедениние
    if (!client.connected()) // Проверка на подключение к MQTT
    {
      while (!client.connected())
      {
        ESP.wdtFeed();                   // Пинок :) "watchdog"
        if (client.connect(name_client)) // имя на сервере mqtt
        {
          client.subscribe(mqtt_reset);  // подписались на топик "ESP8_test_reset"
          client.subscribe(name_client); // подписались на топик
          client.subscribe("temp_zapad");
          client.subscribe("masterskaja_ven_manual");
          client.subscribe("Clock");
          // Отправка IP в mqtt
          char IP_ch[20];
          String IP = (WiFi.localIP().toString().c_str());
          IP.toCharArray(IP_ch, 20);
          client.publish("masterskaja_ventilacija_IP", IP_ch);
        }
        else
        {
          delay(5000);
        }
      }
    }
  }

  if (dht_t.isReady())
  {

    if (dht22.available())
    {
      dht_tik++;
      temp_raw = dht22.readTemperature();
      temp_raw = (temp_raw / 10);
    
      temp_sr = temp_sr + temp_raw;
      if (graf == 60 || graf == 120 || graf == 360)
      {
        temp_sr = temp_sr / dht_tik;
        publish_send("masterskaja_Temper_graf", temp_sr);
        if (graf >= 360)
        {
          graf = 0;
        }
        temp_sr = 0;
        dht_tik = 0;
      }

      // Проветривание по таймеру///////
      if (graf == 0 && manual == 0 && temp_raw > temperatura - 0.5 && provetrivanie.isReady())
      {
        state = 1;
        taimer = 1;
      }
      if (graf == 20 && taimer == 1 && manual == 0)
      {
        taimer = 0;
      }
      // Контроль температуры /////
      if (temp_raw > temperatura && temper_ulica < 19 && manual == 0)
      {
        state = 1;
        provetrivanie.start();
      }

      if (temp_raw < temperatura - 0.3 || temper_ulica > 19)
      {
        if (taimer == 0 && manual == 0)
        {
          state = 0;
        }
      }
      float timer_min = graf;
    }

    publish_send("masterskaja_Temper", temp_raw);
    publish_send("masterskaja_Temper_ul", temper_ulica);
    publish_send("masterskaja_Temper_set", temperatura_set);
    graf++;
  }
}

void setup()
{
  wi_fi_con();
  Serial.begin(9600);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  OTA_Wifi.setInterval(10);        // настроить интервал
  OTA_Wifi.setMode(AUTO);          // Авто режим
  set_manual.setInterval(3600000); // настроить интервал
  set_manual.setMode(MANUAL);      // Авто режим
  provetrivanie.setInterval(3600000);
  provetrivanie.setMode(MANUAL);
  provetrivanie.start();
  ESP.wdtDisable(); // Активация watchdog
  pinMode(D7, OUTPUT);
  dht_t.setInterval(10000); // настроить интервал
  dht_t.setMode(AUTO);      // Авто режим
  dht22.begin();
  temperatura = temperatura_set; // Температура установленная
}
