#include <ESP8266WiFi.h> //Библиотека для работы с WIFI
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> // Библиотека для OTA-прошивки
#include <PubSubClient.h>
#include <TimerMs.h>
#include <SoftwareSerial.h>
#include <Wire.h>
WiFiClient espClient;
PubSubClient client(espClient);
// (период, мс), (0 не запущен / 1 запущен), (режим: 0 период / 1 таймер)
TimerMs OTA_Wifi(10, 1, 0);
TimerMs key(2000, 0, 1);
const char *ssid = "Beeline";                // Имя точки доступа WIFI
const char *name_client = "paradnaja-zamok"; // Имя клиента и сетевого порта для ОТА
const char *password = "sl908908908908sl";   // пароль точки доступа WIFI
const char *mqtt_server = "192.168.1.221";
const char *mqtt_reset = "paradnaja-zamok_reset"; // Имя топика для перезагрузки
String s;
float temperatura_set;
int flag_pub = 1;
byte state = 0, b1 = 0, b2 = 2, b3 = 0, key1 = 0, key2 = 0, key3 = 0;
float hum_raw, temp_raw;
int data;
int graf = 1;
float temper_ulica = 26;

void callback(char *topic, byte *payload, unsigned int length) // Функция Приема сообщений
{
  String s = ""; // очищаем перед получением новых данных
  for (unsigned int i = 0; i < length; i++)
  {
    s = s + ((char)payload[i]); // переводим данные в String
  }

  if ((String(topic)) == "temp_zapad")
  {
    temper_ulica = atof(s.c_str()); // переводим данные в float
  }

  if ((String(topic)) == "masterskaja_ven_manual")
  {
    // state = atof(s.c_str()); // переводим данные в float
    // set_manual.reset();
    // set_manual.start();
    //  manual = 1;
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
///////////////////1//////////////////////
  if (!digitalRead(D7) && b1 == 0)
  {
    key.start();
    tone(D8, 500, 100);
    b1 = 1;
    key1++;
    delay(120);
  }
  else if (digitalRead(D7))
  {
    b1 = 0;
  }
//////////////////2///////////////////////
  if (!digitalRead(D6) && b2 == 0)
  {
    key.start();
    tone(D8, 700, 100);
    b2 = 1;
    key2++;
    delay(120);
  }
  else if (digitalRead(D6))
  {
    b2 = 0;
  }
///////////////////3//////////////////////
  if (!digitalRead(D5) && b3 == 0)
  {
    key.start();
    tone(D8, 1000, 100);
    b3 = 1;
    key3++;
    delay(120);
  }
  else if (digitalRead(D5))
  {
    b3 = 0;
  }


/////////////////////////////////////////////////////////////
    if (!digitalRead(D1))
  {
    tone(D8, 1000, 500);
    digitalWrite(D2, LOW);
    ESP.wdtFeed();
    delay(5000);
    ESP.wdtFeed();
    delay(5000);
    tone(D8, 300, 500);
    digitalWrite(D2, HIGH);
  }
 
  ///////////////////////////////////////////////
  if (key1 == 2 && key2 == 1 && key3 == 4 && key.tick())
  {
    digitalWrite(D2, LOW);
    tone(D8, 300, 100);
    delay(100);
    tone(D8, 1000, 100);
    delay(100);
    tone(D8, 2000, 100);
    delay(100);
    tone(D8, 500, 100);
    delay(100);
    tone(D8, 600, 100);
    delay(100);
    tone(D8, 700, 100);
    ESP.wdtFeed();
    delay(5000);
    digitalWrite(D2, HIGH);
    tone(D8, 2000, 100);
    delay(100);
    tone(D8, 200, 100);
    key1 = 0;
    key2 = 0;
    key3 = 0;
    Serial.println(key1);
  }
  else if (key.tick())
  {
    tone(D8, 1000, 1000);
    key1 = 0;
    key2 = 0;
    key3 = 0;
    Serial.println(key1);
  }

  ESP.wdtFeed();

  if (OTA_Wifi.tick()) // Поддержание "WiFi" и "OTA"  и Пинок :) "watchdog" и подписка на "Топики Mqtt"
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
                                         // client.subscribe("temp_zapad");
          // Отправка IP в mqtt
          char IP_ch[20];
          String IP = (WiFi.localIP().toString().c_str());
          IP.toCharArray(IP_ch, 20);
          client.publish(name_client, IP_ch);
        }
        else
        {
          delay(5000);
        }
      }
    }
  }
}

void setup()
{
  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH);
  wi_fi_con();
  Serial.begin(9600);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  ESP.wdtDisable(); // Активация watchdog
  pinMode(D1, INPUT); // Внутренняя кнопка
  pinMode(D5, INPUT); // 1
  pinMode(D6, INPUT); // 1
  pinMode(D7, INPUT); // 1
  tone(D8, 1000, 500);
}
