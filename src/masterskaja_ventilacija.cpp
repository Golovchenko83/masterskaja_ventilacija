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
GTimer_ms Status;
const char *ssid = "Beeline";                        // Имя точки доступа WIFI
const char *name_client = "masterskaja-ventilacija"; // Имя клиента и сетевого порта для ОТА
const char *password = "sl908908908908sl";           // пароль точки доступа WIFI
const char *mqtt_server = "192.168.1.221";
const char *mqtt_reset = "masterskaja-ventilacija_reset"; // Имя топика для перезагрузки
String s;
int flag_pub = 1;
byte state = 0, taimer = 0;
float hum_raw, temp_raw;
int data;
int graf = 0;
char send[5];
float temper_ulica = 26;

void callback(char *topic, byte *payload, unsigned int length) // Функция Приема сообщений
{
  String s = ""; // очищаем перед получением новых данных
  for (unsigned int i = 0; i < length; i++)
  {
    s = s + ((char)payload[i]); // переводим данные в String
    send[i] = payload[i];
  }

  if ((String(topic)) == "temp_zapad")
  {
    client.publish("master_temp", send, 1);
    temper_ulica = atof(s.c_str()); // переводим данные в float
  }

  int data = atoi(s.c_str()); // переводим данные в int
  // float data_f = atof(s.c_str()); //переводим данные в float
  if ((String(topic)) == mqtt_reset && data == 1)
  {
    ESP.restart();
  }

  if ((String(topic)) == name_client && data == 1)
  {
    digitalWrite(D7, HIGH);
    state = 1;
  }
  if ((String(topic)) == name_client && data == 0)
  {
    digitalWrite(D7, LOW);
    state = 0;
  }
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
  if (Status.isReady())
  {
    if (state)
    {
      client.publish(name_client, "1");
    }
    else
    {
      client.publish(name_client, "0");
    }
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
      if (graf == 105)
        {
          digitalWrite(D7, LOW);
          state = 0;
          taimer = 0;
        }
    graf++;

    if (dht22.available())
    {
      temp_raw = dht22.readTemperature();
      hum_raw = dht22.readHumidity();
      temp_raw = (temp_raw / 10) - 2.5;
      hum_raw = hum_raw / 10;

      if (graf == 300 || graf == 600)
      {
        publish_send("masterskaja_Temper_graf", temp_raw);
        if (graf == 600)
        {
          graf = 0;
        }
      }

      if (temper_ulica < 21)
      {
        if (temp_raw > 23)
        {
          digitalWrite(D7, HIGH);
          state = 1;
        }
        taimer = 0;
      }
      else
      {
        if (graf == 5)
        {
          digitalWrite(D7, HIGH);
          state = 1;
          taimer = 1;
        }
      
      }

      if ((temp_raw < 22.5 || temper_ulica > 21)  && taimer == 0 )
      {
        digitalWrite(D7, LOW);
        state = 0;
      }

      publish_send("masterskaja_Hum", hum_raw);
      publish_send("masterskaja_Temper", temp_raw);
    }
  }
}

void setup()
{
  wi_fi_con();
  Serial.begin(9600);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  OTA_Wifi.setInterval(10); // настроить интервал
  OTA_Wifi.setMode(AUTO);   // Авто режим
  Status.setInterval(3000); // настроить интервал
  Status.setMode(AUTO);     // Авто режим
  ESP.wdtDisable();         // Активация watchdog
  pinMode(D7, OUTPUT);
  dht_t.setInterval(3000); // настроить интервал
  dht_t.setMode(AUTO);     // Авто режим
  dht22.begin();
}
