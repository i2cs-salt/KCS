#include <WiFi.h>
#include "RCS620S.h"
#include <ESP_servo.h>
#include <HTTPClient.h>
#include <ESP.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "LiquidCrystal_I2C.h"

//wifi関連
const char ssid[] = "";
const char password[] = "";
#define esp_HostName ""
#define OTA_pass ""
HTTPClient http;
WiFiServer server(80);
char host[] = "";
const int httpPort = 80;
String insertURL = "";
String requestIDm = "";
String unknownURL = "";

//Felica関連
#define COMMAND_TIMEOUT 50
#define POLLING_INTERVAL 100
#define IMG_BUF 4096
RCS620S rcs620s;
String IDm_list[300]; //IDm照合リスト

//servo関連
ESP_servo servo;

//pin関連
#define led_pin 02
int btnPin = 33, doorPin = 25;
int btnval = 0, doorval = 0;

//液晶関連
LiquidCrystal_I2C lcd(0x27, 16, 2);
int print_flag = 0;
#define I2CADDR_lcd (0x27)

//プロトタイプ宣言
void wificonnect();
String http_try(String URL, String body);
void IDm_request();
void FelicaInit();
void CheakIDm();
void insert_request(int IDm_id);
void unknown_insert(String IDm);
void mDNS_setup();

void door();
void door_led();
void led_flash(int interval, int count);
void display(String message1, String messeage2);
void nomal_display();
void error();

//大域変数等
int http_code = 0, i = 0, error_flag = 0, wifi_status, wifiErCnt = 0;
//ERROR
int httpStaER = 0, wifiConER = 0, mdnsER = 0, idmER = 0;

void wificonnect()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    display("CONNECTING...", "");
    delay(100);
    led_flash(0, 1);
  }
}

void mDNS_setup()
{

  if (MDNS.begin("KCS"))
  {
    display("mDNS OK!", "NAME:KCS");
    server.begin();
  }
  delay(500);
}

String http_try(String URL, String body)
{
  delay(50);
  String response = "";
  http.begin(URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http_code = http.POST(body);
  delay(50);
  if (http_code != 200)
  {
    httpStaER = 1;
    return "";
  }else{
    httpStaER = 0;
  }
  response = http.getString();
  http.end();
  return response;
}

void IDm_request()
{
  String response;
  String message1 = "http_code:";
  String message2 = "IDm: ";
  int end = 0, begin = 0, i = 1, last = 0;

  display("IDm REQUESTING", "");
  response = http_try(requestIDm, "");
  delay(10);
  message1.concat(http_code);
  display(message1, "");
  delay(500);
  display("Now storing", "");
  delay(10);
  begin = response.indexOf("{", 0);
  while (begin == -1)
  {
    display("FAILED TO GET","RETRYING NOW");
    response = http_try(requestIDm, "");
    begin = response.indexOf("{", 0);
    led_flash(0, 1);
  }
  begin++;
  last = response.indexOf(",}", 0);
  String tmp = "last: ";

  while (true)
  {
    if(i%2==0){
      end = response.indexOf(",", begin);
      IDm_list[i] = response.substring(begin, end);
      i++;
      end++;
      begin = end;
      if (begin >= last)
      {
        break;
      }
      else if (i == 100)
      {
        idmER = 1;
      }
      delay(10);
    }else{
      end = response.indexOf(",", begin);
      IDm_list[i] = response.substring(begin, end);
      i++;
      end++;
      begin = end;
    }
   }
}

void FelicaInit()
{
  int ret;
  display("Felica", "INITIALIZING...");
  ret = rcs620s.initDevice();
  while (!ret)
  {
    ret = rcs620s.initDevice(); // blocking
  }
}

void CheckIDm()
{
  String IDm = "";
  char buf[3];
  int IDm_false = 1;
  int IDm_id;

  for (int i = 0; i < 8; i++)
  {
    sprintf(buf, "%02X", rcs620s.idm[i]);
    IDm.concat(buf);
  }
  for (i = 1; i <= 300; i++)
  {
    if (IDm == IDm_list[i])
    {
      IDm_id = IDm_list[i-1].toInt();
      door();
      insert_request(IDm_id);
      delay(100);
      IDm_false = 0;
      break;
    }
  }
  if (IDm_false)
  {
    unknown_insert(IDm);
  }
  print_flag = 0;
}

void insert_request(int IDm_id)
{
  String id = "id=";
  wifi_status = WiFi.status();
  if (wifi_status == 3)
  {
    display("LOG WRITING", "");
    id.concat(IDm_id);
    http_try(insertURL, id);
    display("COMPLETE!", "");
  }else
  {
    wifiConER = 1;
  }
}

void unknown_insert(String IDm)
{
  String id = "IDm=";

  wifi_status = WiFi.status();
  if (wifi_status == 3)
  {
    display("LOG WRITING", "");
    id.concat(IDm);
    http_try(unknownURL, id);
    display("UNKNOWN", "");
    delay(3000);
  }else
  {
    wifiConER = 1;
  }
  
}

void door()
{
  int i = 0;

  print_flag = 0;
  display("Key:UNLOCKED", "");
  servo.write(0);
  delay(250);
  door_led();
  delay(750);
  servo.write(90);
  delay(2000);
  while (i < 3000)
  {
    door_led();
    delay(1);
    i++;
  }
  while (true)
  {
    i = 0;
    if (digitalRead(doorPin))
    {
      while (i < 2000)
      {
        door_led();
        if (!digitalRead(doorPin))
        {
          i = 0;
          continue;
        }
        delay(1);
        i++;
      }

      if (digitalRead(doorPin))
      {
        door_led();
        servo.write(180);
        delay(1000);
        servo.write(90);
        break;
      }
      else
      {
        continue;
      }
    }
  }
}

void led_flash(int interval, int count)
{
  int i = 0;
  for (i = 0; i < count; i++)
  {
    digitalWrite(led_pin, !digitalRead(led_pin));
    delay(interval);
  }
}

void display(String message1, String message2)
{
  lcd.clear();
  delay(50);
  lcd.setCursor(0, 0);
  lcd.print(message1);
  lcd.setCursor(0, 1);
  lcd.print(message2);
}

void nomal_display()
{
  if (print_flag == 0)
  {
    display("KCSver2.2", "STATUS:OK");
    print_flag = 1;
  }
}

void error()
{
  String msg1 = "ERROR:", msg2 = "";
  int msg1flag = 0, msg2flag = 0; //「,」をつけるかいなか

  if(wifiConER){
    if(wifiErCnt == 50 || wifiErCnt == 1){
      WiFi.begin();
      wifiErCnt = 0;
    }else{
      wifiErCnt++;
    }
  }

  if (httpStaER)
  {
    msg1.concat("http");
    msg2.concat(http_code);
    msg1flag = 1;
    msg2flag = 1;
  }

  if (mdnsER && msg1flag)
  {
    msg1.concat(",mdns");
  }
  else if (mdnsER)
  {
    msg1.concat("mdns");
    msg1flag = 1;
  }

  if (idmER && msg1flag)
  {
    msg1.concat(",IDm");
  }
  else if (idmER)
  {
    msg1.concat("IDm");
    msg1flag = 1;
  }

  if (msg1flag)
  {
    display(msg1, msg2);
  }
}

void door_led()
{
  if (digitalRead(doorPin) != 0)
  {
    digitalWrite(led_pin, HIGH);
  }
  else
  {
    digitalWrite(led_pin, LOW);
  }
}


void setup()
{
  delay(200);
  pinMode(led_pin, OUTPUT);
  pinMode(btnPin, INPUT);
  pinMode(doorPin, INPUT);
  
  Serial.begin(115200);
  Serial2.begin(115200); // for RC-S620/S
  delay(500);
  servo.init(13, 0);
  lcd.init();
  lcd.backlight();
  delay(100);

  display("WELCOME", "");

  wificonnect();
  led_flash(50, 30);
  digitalWrite(led_pin, LOW);

  IDm_request();
  led_flash(50, 30);
  delay(500);
  
  FelicaInit();
  delay(500);
  digitalWrite(led_pin, LOW);
  
  mDNS_setup();

  ArduinoOTA.setHostname(esp_HostName);
  ArduinoOTA.setPassword(OTA_pass);

  ArduinoOTA
      .onStart([]() { //OTAアップデート開始時の関数
        String type;

        display("OTA:START", "");
        delay(500);
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
      })
      .onEnd([]() { //OTAアップデート終了時の関数
        display("End", "");
        delay(500);
      })
      .onProgress([](unsigned int progress, unsigned int total) { //OTAアップデート実行中の関数
        String message1 = "Progress: ";
        double score;
        score = progress / (total / 100);
        message1.concat(score);
        display("NOW UPDATING", message1);
      })
      .onError([](ota_error_t error) { //OTAアップデート失敗時の関数
        if (error == OTA_AUTH_ERROR)
          display("Auth Failed", "");
        else if (error == OTA_BEGIN_ERROR)
          display("Begin Failed", "");
        else if (error == OTA_CONNECT_ERROR)
          display("Connect Failed", "");
        else if (error == OTA_END_ERROR)
          display("End Failed", "");
        else if (error == OTA_RECEIVE_ERROR)
          display("Receive Failed", "");
      });

  ArduinoOTA.begin();

  display("KCS READY", "");
  delay(500);
}

void loop()
{
  int ret; // Polling
  int wifi_status;

  rcs620s.timeout = COMMAND_TIMEOUT;
  ret = rcs620s.polling();

  if (ret)
  {
    CheckIDm();
  }

  if (digitalRead(btnPin) != 0)
  {
    door();
  }

  nomal_display();

  error();

  door_led();

  rcs620s.rfOff();
  delay(POLLING_INTERVAL);
  
  ArduinoOTA.handle();
  delay(50);
}
