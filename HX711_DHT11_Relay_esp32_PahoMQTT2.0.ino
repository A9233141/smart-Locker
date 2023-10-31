#include <WiFi.h>
#include <PubSubClient.h>
#include <EasyButton.h>
#include "HX711.h"
#include "DHT.h"
//#include "wifi.h"

#define LED 26        //繼電器接腳
#define Relay 27
#define bezzPin  25 //定義蜂鳴器

//HX711設定
//HX711 HX711_CH0(4, 16, 479); //磅秤接腳 SCK,DT,GapValue:245最穩定、246

const int DT_PIN = 14;
const int SCK_PIN = 12;
const int scale_factor = 484; //比例參數，從校正程式中取得
float Weight = 0;
HX711 scale;



#define DHTPIN 13     // DHT11接腳
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); // 讓DHT11程式庫做初始化

const int updateCycle = 10000;//更新速度，五秒更新一次。最快只能設定2秒一次


const char *ssid = "Alex-4A";
const char *password = "0912235003.";

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
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
char msg1[MSG_BUFFER_SIZE];
char msg2[MSG_BUFFER_SIZE];
int value = 0;


void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

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
      digitalWrite(LED, LOW); //Turn off
      client.publish(pub_led_topic, "LEDoff");
    }
    if (message == "LEDon") {
      digitalWrite(LED, HIGH); //Turn on
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
      digitalWrite(bezzPin, HIGH);
      delay(500);
      digitalWrite(bezzPin, LOW);
      client.publish(pub_relay_topic, "on");
    }
    if (message == "1234567890") {
      digitalWrite(Relay, HIGH); //Turn on
      delay(3000);//等三秒
      digitalWrite(Relay, LOW); //Turn off
      digitalWrite(bezzPin, HIGH);
      delay(500);
      digitalWrite(bezzPin, LOW);
      client.publish(pub_relay_topic, "QRcodeon");
    }
  }

}

void reconnect()
{
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

  Serial.begin(115200); //鮑率設為115200
  setup_wifi();
  //wifi_setup();//呼叫Wifi.h

  dht.begin(); //DHT11開始工作

  pinMode(bezzPin, OUTPUT);

  scale.begin(DT_PIN, SCK_PIN);

  //HX711_CH0.begin();          //讀取傳感器重量，寫在setup
  //delay(3000);                //延時3s用於傳感器穩定，寫在setup
  //HX711_CH0.begin();          //重新讀取傳感器支架毛重用於後續計算，寫在setup

  Serial.println("Before setting up the scale:");
  Serial.println(scale.get_units(5), 0);  //未設定比例參數前的數值
  scale.set_scale(scale_factor);       // 設定比例參數
  scale.tare();               // 歸零
  Serial.println("After setting up the scale:");
  Serial.println(scale.get_units(5), 0);  //設定比例參數後的數值
  Serial.println("Readings:");  //在這個訊息之前都不要放東西在電子稱上

  pinMode(LED, OUTPUT); //設定LED接腳為輸出
  digitalWrite(LED, LOW); //設定初始值為低電壓

  pinMode(Relay, OUTPUT); //設定繼電器接腳為輸出
  digitalWrite(Relay, LOW); //設定初始值為低電壓

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  Serial.println(scale.get_units(10), 0);
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();//取得目前時間
  if (now - lastMsg > updateCycle) //用現在的時間上減掉上一個訊息所發送的時間看，看有沒有大於updateCycle，如果沒有大5秒的話就不會進入這一個迴圈。
  {

    lastMsg = now;
    Weight = scale.get_units(10);
    if (Weight <= 0) {
      Weight = 0;
    }
    //Weight = HX711_CH0.Get_Weight();    //當前傳感器重量，該重量已經扣除支架重量
    float humidity = dht.readHumidity();//讀取濕度
    float temperature = dht.readTemperature(); //讀取溫度
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) { //確定是否有讀到溫度和濕度

      Serial.println(F("Failed to read from DHT sensor!"));//顯示失偵測再序列埠視窗
      return;

    } else {
      // publish the message
      snprintf(msg, MSG_BUFFER_SIZE, "%.1lf°", temperature);//溫度
      snprintf(msg1, MSG_BUFFER_SIZE, "%.0lf%%", humidity); //濕度
      snprintf(msg2, MSG_BUFFER_SIZE, "%.1lfg" , Weight ); //重量

      Serial.print("Publish message: ");//發布訊息
      Serial.println(msg);//從MCU上發布溫度訊息顯示再序列埠視窗
      Serial.println(msg1);//從MCU上發布濕度訊息顯示再序列埠視窗
      Serial.println(msg2);//從MCU上發布重量訊息顯示再序列埠視窗

      client.publish(pub_temp_topic, msg);//從MCU上發布溫度訊息
      client.publish(pub_humd_topic, msg1);//從MCU上發布濕度訊息
      client.publish(pub_HHX711_topic, msg2);//從MCU上發布重量訊息

    }

  }
}
