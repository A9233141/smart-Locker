//======================================== Including the libraries.
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Pushbutton.h>
#include "DHT.h"
#include "HX711.h"
#include "time.h"
//--------------------//OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED 寬度像素
#define SCREEN_HEIGHT 64 // OLED 高度像素
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//======================================== Time
unsigned long lastCheck = 0;
const unsigned long checkInterval = 8000; // 5 seconds
//======================================== HX711 sensor setting (HX711)
const int LOADCELL_DOUT_PIN = 14;
const int LOADCELL_SCK_PIN = 12;
#define CALIBRATION_FACTOR 392.2925170068027
HX711 scale;
bool messageSent = false;
//======================================== DHT sensor settings (DHT11).
#define DHTPIN 13 //--> Defines the Digital Pin connected to the DHT11 sensor.
#define DHTTYPE DHT11 //--> Defines the type of DHT sensor used. Here used is the DHT11 sensor.
DHT dht11_sensor(DHTPIN, DHTTYPE); //--> Initialize DHT sensor.
//======================================== Rest Button
//Rstmsg2dbButton
#define BUTTON_PIN 17
Pushbutton button(BUTTON_PIN);

// RstButton
#define I00_PIN 0
Pushbutton button1(I00_PIN);
//======================================= Defines the Digital Pin of the "On Board LED".
#define ON_Board_LED 2

#define LED_01 13

#define LED_02 12

//======================================== SSID and Password of your WiFi router.
const char* ssid = "Alex-4A";
const char* password = "0912235003.";

//======================================== Variables for HTTP POST request data.
String postData = ""; //--> Variables sent for HTTP POST request data.
String payload = "";  //--> Variable for receiving response from HTTP POST.

//======================================== Variables for DHT11 sensor data.
float send_Temp;
int send_Humd;
int send_Merchant;
String send_Status_Read_DHT11 = "";

//Subroutine to control LEDs after successfully fetching data from database.
void control_LEDs() {
  Serial.println();
  //Serial.println("---------------control_LEDs()");
  JSONVar myObject = JSON.parse(payload);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    //Serial.println("Parsing input failed!");
    Serial.println("---------------");
    return;
  }

  if (myObject.hasOwnProperty("LED_01")) {
    Serial.print("myObject[\"LED_01\"] = ");
    Serial.println(myObject["LED_01"]);
  }

  if (myObject.hasOwnProperty("LED_02")) {
    Serial.print("myObject[\"LED_02\"] = ");
    Serial.println(myObject["LED_02"]);
  }

  if (strcmp(myObject["LED_01"], "ON") == 0)   {
    digitalWrite(LED_01, HIGH);
    Serial.println("LED 01 ON");
  }
  if (strcmp(myObject["LED_01"], "OFF") == 0)  {
    digitalWrite(LED_01, LOW);
    Serial.println("LED 01 OFF");
  }
  if (strcmp(myObject["LED_02"], "ON") == 0)   {
    digitalWrite(LED_02, HIGH);
    Serial.println("LED 02 ON");
  }
  if (strcmp(myObject["LED_02"], "OFF") == 0)  {
    digitalWrite(LED_02, LOW);
    Serial.println("LED 02 OFF");
  }

  Serial.println("---------------");
}
//get data from the HX711 sensor
void get_HX711_sensor_data() {
  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();
    Serial.println();
    Serial.println("重量資訊取得中...");
    if (scale.wait_ready_timeout(200)) {
      send_Merchant = round(scale.get_units());
      Serial.print("Weight: ");
      Serial.println(send_Merchant);
      display.clearDisplay();
      display.setTextSize(2);             // 設定文字大小
      display.setTextColor(1);        // 1:OLED預設的顏色(這個會依該OLED的顏色來決定)

      display.setCursor(0, 0);            // 設定起始座標
      display.print("Box Data: ");        // 要顯示的字串

      display.setCursor(0, 16);            // 設定起始座標
      display.print("Weight:");      //顯示溫度

      display.setCursor(0, 47);           // 設定起始座標
      display.print(send_Merchant); //重量的數值
      display.print(" g");

      display.display();                  // 要有這行才會把文字顯示出來
    }
  }
}
//======================================== REST Fun
void Hx711RstMsg2db() {
  if (button.getSingleDebouncedPress()) {
    messageSent = false;
    Serial.println("RST...");
    display.clearDisplay();
    display.setTextSize(2);             // 設定文字大小
    display.setTextColor(1);        // 1:OLED預設的顏色(這個會依該OLED的顏色來決定)

    display.setCursor(0, 0);            // 設定起始座標
    display.print("Box Data: ");        // 要顯示的字串

    display.setCursor(0, 16);            // 設定起始座標
    display.print("RST...");
    display.display();                  // 要有這行才會把文字顯示出來
  }
}
void Hx711rst() { //去皮
  if (button1.getSingleDebouncedPress()) {
    Serial.print("0...");
    scale.tare();
    display.clearDisplay();
    display.setTextSize(2);             // 設定文字大小
    display.setTextColor(1);        // 1:OLED預設的顏色(這個會依該OLED的顏色來決定)

    display.setCursor(0, 0);            // 設定起始座標
    display.print("Box Data: ");        // 要顯示的字串

    display.setCursor(0, 16);            // 設定起始座標
    display.print("0...");
    display.display();                  // 要有這行才會把文字顯示出來
  }
}
//======================================== Subroutine to read and get data from the DHT11 sensor.
void get_DHT11_sensor_data() {
  Serial.println();
  Serial.println("-------------get_DHT11_sensor_data()");

  send_Temp = dht11_sensor.readTemperature();
  send_Humd = dht11_sensor.readHumidity();

  if (isnan(send_Temp) || isnan(send_Humd)) {
    Serial.println("Failed to read from DHT sensor!");
    send_Temp = 0.00;
    send_Humd = 0;
    send_Status_Read_DHT11 = "FAILED";
  } else {
    send_Status_Read_DHT11 = "SUCCEED";
  }
  Serial.printf("Temperature : %.2f °C\n", send_Temp);
  Serial.printf("Humidity : %d %%\n", send_Humd);
  Serial.printf("Status Read DHT11 Sensor : %s\n", send_Status_Read_DHT11);
  Serial.println("-------------");
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200); //--> Initialize serial communications with the PC.
  //-------------------------------偵測是否安裝好OLED了
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 一般1306 OLED的位址都是0x3C
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);// Don't proceed, loop foreve
  }
  // 顯示Adafruit的LOGO，算是開機畫面
  display.display();
  delay(500); // 停0.5秒

  // 清除畫面
  display.clearDisplay();
  //-------------------------------
  pinMode(ON_Board_LED, OUTPUT); //--> On Board LED port Direction output.
  pinMode(LED_01, OUTPUT); //--> LED_01 port Direction output.
  pinMode(LED_02, OUTPUT); //--> LED_02 port Direction output.

  digitalWrite(ON_Board_LED, HIGH); //--> Turn on Led On Board.
  digitalWrite(LED_01, HIGH); //--> Turn on LED_01.
  digitalWrite(LED_02, HIGH); //--> Turn on LED_02.

  delay(2000);

  digitalWrite(ON_Board_LED, LOW); //--> Turn off Led On Board.
  digitalWrite(LED_01, LOW); //--> Turn off Led LED_01.
  digitalWrite(LED_02, LOW); //--> Turn off Led LED_02.

  //---------------------------------------- Make WiFi on ESP32 in "STA/Station" mode and start connecting to WiFi Router/Hotspot.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //----------------------------------------

  Serial.println();
  Serial.println("-------------");
  Serial.print("Connecting");


  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    //........................................ Make the On Board Flashing LED on the process of connecting to the wifi router.
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
    digitalWrite(ON_Board_LED, LOW);
    delay(250);


    //........................................ Countdown "connecting_process_timed_out".
    if (connecting_process_timed_out > 0) connecting_process_timed_out--;
    if (connecting_process_timed_out == 0) {
      delay(1000);
      ESP.restart();
    }
  }


  digitalWrite(ON_Board_LED, LOW); //--> Turn off the On Board LED when it is connected to the wifi router.

  //---------------------------------------- If successfully connected to the wifi router, the IP Address that will be visited is displayed in the serial monitor
  Serial.println();
  Serial.print("確認商家端測量平台已連上網路SSID是 : ");
  Serial.println(ssid);
  Serial.print("IP位置是: ");
  Serial.println(WiFi.localIP());
  Serial.print("商家人員請確認 ");
  Serial.println("-------------");

  // Setting up the DHT sensor (DHT11).
  dht11_sensor.begin();

  // Setting up the HX711 sensor (DHT11).
  //Serial.println("Initializing the scale");//初始化稱重傳感器
  Serial.println("已連接至MySQL資料庫esp32_dht11_record...");
  Serial.println("------------------------------");
  Serial.println("磅秤已初始化完成，可以開始使用");//初始化稱重傳感器
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);//寫在setup
  scale.set_scale(CALIBRATION_FACTOR);//寫在setup
  scale.tare();//寫在setup，歸零。

  delay(2000);
}

void loop() {
  get_HX711_sensor_data();
  Hx711RstMsg2db();
  Hx711rst();
  if (send_Merchant >= 100 && !messageSent) {

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;  //--> Declare object of class HTTPClient.
      int httpCode;     //--> Variables for HTTP return code.

      postData = "id=esp32_01";
      payload = "";

      digitalWrite(ON_Board_LED, HIGH);
      Serial.println();
      Serial.println("資料已記錄至MySQL資料庫...");
      display.clearDisplay();
      display.setTextSize(2);             // 設定文字大小
      display.setTextColor(1);        // 1:OLED預設的顏色(這個會依該OLED的顏色來決定)

      display.setCursor(0, 0);            // 設定起始座標
      display.print("Set json..");        // 要顯示的字串
      display.display();                  // 要有這行才會把文字顯示出來


      // Example : http.begin("http://172.20.10.4/ESP32_MySQL_Database/Final/getdata.php");
      http.begin("http://192.168.137.1/ESP32_MySQL_Database/Final3/getdata.php");  //--> Specify request destination
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");        //--> Specify content-type header

      httpCode = http.POST(postData); //--> Send the request
      payload = http.getString();     //--> Get the response payload

      Serial.print("httpCode : ");
      Serial.println(httpCode); //--> Print HTTP return code
      Serial.print("payload  : ");
      Serial.println(payload);  //--> Print request response payload

      http.end();  //--> Close connection
      Serial.println("---------------");
      digitalWrite(ON_Board_LED, LOW);
      //........................................

      // Calls the control_LEDs() subroutine.
      control_LEDs();

      delay(1000);

      // Calls the get_DHT11_sensor_data() subroutine.
      get_DHT11_sensor_data();
      get_HX711_sensor_data();


      String LED_01_State = "";
      String LED_02_State = "";

      if (digitalRead(LED_01) == 1) LED_01_State = "ON";
      if (digitalRead(LED_01) == 0) LED_01_State = "OFF";
      if (digitalRead(LED_02) == 1) LED_02_State = "ON";
      if (digitalRead(LED_02) == 0) LED_02_State = "OFF";

      postData = "id=esp32_01";
      postData += "&temperature=" + String(send_Temp);
      postData += "&humidity=" + String(send_Humd);
      postData += "&Merchant=" + String(send_Merchant);
      postData += "&status_read_sensor_dht11=" + send_Status_Read_DHT11;
      postData += "&led_01=" + LED_01_State;
      postData += "&led_02=" + LED_02_State;
      payload = "";

      digitalWrite(ON_Board_LED, HIGH);
      Serial.println();
      Serial.println("---------------updateDHT11data_and_recordtable.php");
      // Example : http.begin("http://192.168.0.0/ESP32_MySQL_Database/Final/updateDHT11data_and_recordtable.php");
      http.begin("http://192.168.137.1/ESP32_MySQL_Database/Final3/updateDHT11data_and_recordtable.php");  //--> Specify request destination
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //--> Specify content-type header

      httpCode = http.POST(postData); //--> Send the request
      payload = http.getString();  //--> Get the response payload

      Serial.print("httpCode : ");
      Serial.println(httpCode); //--> Print HTTP return code
      Serial.print("payload  : ");
      Serial.println(payload);  //--> Print request response payload

      http.end();  //Close connection
      Serial.println("---------------");
      digitalWrite(ON_Board_LED, LOW);
    }
    messageSent = true;
  }
}
