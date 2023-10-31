#include <PubSubClient.h>
#include <Pushbutton.h>
#include <EasyButton.h>

#ifdef ESP32
#include <WiFi.h>
int ledOn = 1;
int ledOff = 0;
#else

#include <ESP8266WiFi.h>
int ledOn = 0;
int ledOff = 1;
#endif

#ifdef ESP8266
String  ESPtype = "ESP8266-" + String(ESP.getChipId());
#else
String  ESPtype = "ESP32";
#endif

#include <TridentTD_LineNotify.h>  // Line Notify專用的引入函示庫
#include <NTPClient.h>      // 國際標準時間網路擷取函示庫
#include <WiFiUdp.h>

#define sendEnPin 27      // 定時發送選擇控制腳
#define LED 2
#define RelayLED 26        //繼電器接腳
#define Relay 25
//#define bezzPin  27 //定義蜂鳴器

//Button
#define I00_PIN 0
Pushbutton button(I00_PIN);

//HX711設定
#include "HX711.h"
//HX711 HX711_CH0(4, 16, 476); //磅秤接腳 SCK,DT,GapValue:245最穩定、246、479.2896174863388
const int LOADCELL_DOUT_PIN = 14;
const int LOADCELL_SCK_PIN = 12;
#define CALIBRATION_FACTOR  484.505194095134
HX711 scale;
int Weight;

//DHT11設定
#include "DHT.h"
#define DHTPIN 13     // DHT22接腳
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE); // 讓DHT22程式庫做初始化

//MQTT資料上傳速度、WiFi的設定
const int updateCycle = 10000;//更新速度，五秒更新一次。最快只能設定2秒一次
const char *ssid = "Alex-4A";
const char *password = "0912235003.";
#define LINE_TOKEN  "d0pS7WVanMQzXtV83k96cUdKJF922XFm7SKRuSyv4Ld"

// 填上自己喜愛的裝置名稱，盡量獨一無二
String mqtt_ClientID = " zxcvbn546674_IOT_";

// 寫出我們需要訂閱/發不的主題
const char* sub_topic = "YourEsp32/esp32s";//向伺服器訂閱以控制硬體的狀態
const char* pub_led_topic = "YourEsp32/esp32s_led_state";//向伺服器發布LED的狀態
const char* pub_relay_topic = "YourEsp32/esp32s_relay_state";//向伺服器發布繼電器的狀態
const char* pub_init_topic = "YourEsp32/esp32s_is_back";//向伺服器發布MCU上線的訊息
const char* pub_temp_topic = "YourEsp32/esp32s_temp"; //向伺服器發布溫度訊息
const char* pub_humd_topic = "YourEsp32/esp32s_humd"; //向伺服器發布濕度訊息
const char* pub_HHX711_topic = "YourEsp32/esp32s_HHX711"; //向伺服器發布重量訊息

// MQTT伺服器的選擇
const char *mqtt_server = "mqttgo.io";
const char *mqtt_userName = "";
const char *mqtt_password = "";

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");   // 國際標準時間網站
float humLimit = 90;    // 濕度警示上限值
float tempLimit = 40;  // 溫度警示上限值
int weight = 100;     //重量警示上限值

unsigned long currentTime;
int count = 0;
int sendTime = 60;      // DHT22量測結果發送時間以秒為單位
int checkTime = 30;      // DHT22量測結果檢查時間以秒為單位

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
char msg1[MSG_BUFFER_SIZE];
char msg2[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  bool outState = false;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    outState = !outState;
    if (outState)
      digitalWrite(LED, ledOn);
    else
      digitalWrite(LED, ledOff);
    delay(500);
  }

  randomSeed(micros());
  digitalWrite(LED, ledOn);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  payload[length] = '\0';
  String message = (char *)payload;

  if (strcmp(topic, sub_topic) == 0) {
    if (message == "LEDoff") {
      digitalWrite(RelayLED, LOW); //Turn off
      client.publish(pub_led_topic, "LEDoff");
    }
    if (message == "LEDon") {
      digitalWrite(RelayLED, HIGH); //Turn on
      client.publish(pub_led_topic, "LEDon");
    }
  }

  if (strcmp(topic, sub_topic) == 0) {
    if (message == "Relayoff") {
      digitalWrite(Relay, LOW); //Turn off
      client.publish(pub_relay_topic, "off");
    }
    if (message == "Relayon") {
      digitalWrite(Relay, HIGH); //Turn on
      //digitalWrite(Relay, HIGH); //Turn on
      //delay(3000);
      //digitalWrite(Relay, LOW); //Turn off
      //digitalWrite(bezzPin, HIGH);
      //delay(500);
      //digitalWrite(bezzPin, LOW);
      client.publish(pub_relay_topic, "on");
    }
    if (message == "1234567890") {
      digitalWrite(Relay, HIGH); //Turn on
      delay(3000);//等三秒
      digitalWrite(Relay, LOW); //Turn off
      //digitalWrite(bezzPin, HIGH);
      //delay(500);
      //digitalWrite(bezzPin, LOW);
      client.publish(pub_relay_topic, "QRcodeon");
    }
  }

}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected())
  {

    Serial.println("Attempting mqttgo.io MQTT connection...");
    // Create a random client ID
    mqtt_ClientID += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect((mqtt_ClientID, mqtt_userName, mqtt_password)))
    {
      Serial.print(" connected with Client ID: ");
      Serial.println(mqtt_ClientID);
      // Once connected, publish an announcement...
      client.publish(pub_init_topic, "Hi, I'm online!");
      // ... and resubscribe
      client.subscribe(sub_topic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup() {
  Serial.begin(115200); Serial.println();
  setup_wifi(); //呼叫Wifi涵式
  dht.begin(); //DHT22開始工作

  //HX711磅秤的啟動
  Serial.println("Initializing the scale");//初始化稱重傳感器
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);//寫在setup
  scale.set_scale(CALIBRATION_FACTOR);//寫在setup
  scale.tare();//寫在setup，歸零。

  //MCU連接成功後會序列埠出現的訊息
  Serial.println(LINE.getVersion());
  Serial.print("目前使用的模組為 => ");
  Serial.println(ESPtype);
  timeClient.begin();
  timeClient.setTimeOffset(28800);  // 加上本地時間(距國際標準格林威治時間8小時)偏差的補償值

  //  初始化Line Token
  LINE.setToken(LINE_TOKEN);
  Serial.println("開始傳送外賣箱資料!");
  LINE.notify("開始傳送外賣箱資料!");  // 系統啟動後先送出一提示訊息給Line使用者提醒系統已經開始工作
  delay(2000);
  currentTime = millis();     // 取得我們系統目前的時間(以mS為單位)

  //pinMode(bezzPin, OUTPUT);

  pinMode(LED, OUTPUT);
  pinMode(sendEnPin, INPUT_PULLUP);

  pinMode(RelayLED, OUTPUT); //設定LED接腳為輸出
  digitalWrite(RelayLED, LOW); //設定初始值為低電壓

  pinMode(Relay, OUTPUT); //設定繼電器接腳為輸出
  digitalWrite(Relay, LOW); //設定初始值為低電壓

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
void HXX711() {
  if (scale.wait_ready_timeout(200)) {
    Weight = round(scale.get_units());
    Serial.print("Weight: ");
    Serial.println(Weight);
  }
}
void Hx711rst() {
  if (button.getSingleDebouncedPress()) {
    Serial.print("tare...");
    scale.tare();
  }
}
void LineBot() {
  // 先測試是否已到達檢查環境溫濕度的時間：

  if ((millis() - currentTime) >= checkTime * 1000)

  {
    count += checkTime;
    currentTime = millis();
    timeClient.update();  // 取得目前的格林威治標準時間

    // 取出其中帶有紀元時間中（自 1970 年 1 月 1 日 格林威治標準時間午夜以來)經過的秒數；
    unsigned long epochTime = timeClient.getEpochTime();

    //  轉換並取得一時間結構資料:
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;      // 取得目前日期中的天數
    int currentMonth = ptm->tm_mon + 1;   // 取得目前日期中的月份
    int currentYear = ptm->tm_year + 1900;  // 取得目前日期中的年

    //輸出年月日的資料:
    String currentDate = String(currentYear) + "/" + String(currentMonth) + "/" + String(monthDay);

    // 將前面取得的目前日期的年月日轉成一個日期字串
    String formattedTime = timeClient.getFormattedTime();
    String msg = "測量的日期與時間為: " + currentDate + "  " + formattedTime + "\n";


    HXX711();
    if (Weight <= 0) {
      Weight = 0;
    }
    float  hum = dht.readHumidity();    // 取得感測器所測量的濕度值
    float  tem = dht.readTemperature();   // 取得感測器所測量的溫度值


    msg += ESPtype + "上的DHT22:\n溫度 = " + String(tem) + " 'C\n";
    msg += "濕度 = " + String(hum) + " %\n";
    msg += "重量 =" + String(Weight) + "g\n";
    String  hint = "";

    if (tem >= tempLimit)
      hint = "!!! 溫度超過上限值 !!!\n";

    if (hum >= humLimit)
      hint += "!!! 濕度超過上限值 !!!\n";

    if (Weight >= weight)
      hint += "!!! 重量已改變 !!!\n";


    // 測試目前的溫/溼度值是否超標
    if ((hum >= humLimit) || (tem >= tempLimit) || (Weight >= weight)) {

      msg = hint + msg; // 將警示訊息合併到溫/溼度量測值訊息
      LINE.notify(msg);    // 若超過警戒值則發送警示訊息到對應的Line群組

    }

    if (count >= sendTime)  // 測試是否已達定時發送的時間
    {

      if (digitalRead(sendEnPin) == 0)    // 測試定時發送功能是否被致能
        LINE.notify(msg); // 若是則將量測到的溫/濕度訊息發送到Line群組

      // 將將量測到的溫/濕度訊息顯示在序列監控視窗上
      Serial.print("今天的日期: ");
      Serial.println(currentDate);
      Serial.print("現在時間是: ");
      Serial.println(formattedTime);
      Serial.println("外賣箱測量結果為");
      Serial.print("重量 = ");
      Serial.print(Weight);
      Serial.println("g");
      Serial.print("溫度 = ");
      Serial.print(tem);
      Serial.println("*C");
      Serial.print("濕度 = ");
      Serial.print(hum);
      Serial.print("%\t");
      Serial.println();
      count = 0;
    }

  }
}
void loop() {
  Hx711rst();
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();//取得目前時間
  if (now - lastMsg > updateCycle) //用現在的時間上減掉上一個訊息所發送的時間看，看有沒有大於updateCycle，如果沒有大5秒的話就不會進入這一個迴圈。
  {

    lastMsg = now;//
    //Weight = HX711_CH0.Get_Weight();    //當前傳感器重量，該重量已經扣除支架重量
    HXX711();
    if (Weight <= 0) {
      Weight = 0;
    }
    float humidity = dht.readHumidity();//讀取濕度
    float temperature = dht.readTemperature(); //讀取溫度
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) { //確定是否有讀到溫度和濕度

      Serial.println(F("Failed to read from DHT sensor!"));//顯示失偵測再序列埠視窗
      return;

    } else {
      // publish the message
      snprintf(msg, MSG_BUFFER_SIZE, "%.1lf", temperature);//溫度
      snprintf(msg1, MSG_BUFFER_SIZE, "%.0lf", humidity); //濕度
      snprintf(msg2, MSG_BUFFER_SIZE, "%d" , Weight ); //重量
      //snprintf(msg2, MSG_BUFFER_SIZE, "%.1lfg" , Weight ); //重量

      Serial.print("Publish message: ");//發布訊息
      Serial.println(msg);//從MCU上發布溫度訊息顯示再序列埠視窗
      Serial.println(msg1);//從MCU上發布濕度訊息顯示再序列埠視窗
      Serial.println(msg2);//從MCU上發布重量訊息顯示再序列埠視窗

      client.publish(pub_temp_topic, msg);//從MCU上發布溫度訊息
      client.publish(pub_humd_topic, msg1);//從MCU上發布濕度訊息
      client.publish(pub_HHX711_topic, msg2);//從MCU上發布重量訊息

    }

  }
  LineBot();
}
