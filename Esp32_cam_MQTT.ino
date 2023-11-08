#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>//請先安裝PubSubClient程式庫

#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "FS.h"
#include "SD_MMC.h"            // SD Card ESP32
#include <EEPROM.h>            // read and write from flash memory
#define EEPROM_SIZE 1

int pictureNumber = 0; //照片存檔編號
int vInitSD_OK = 1; //記憶卡是否可用
int vTakePicture = -1; //拍照是否成功

#include <TJpg_Decoder.h>

#ifdef USE_DMA
uint16_t  dmaBuffer1[16 * 16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t  dmaBuffer2[16 * 16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t* dmaBufferPtr = dmaBuffer1;
bool dmaBufferSel = 0;
#endif

#include "SPI.h"
#include <TFT_eSPI.h>              // Hardware-specific library
TFT_eSPI tft = TFT_eSPI();         // Invoke custom library

sensor_t * s;
int pTFT_Stop = 0;

char* ssid = "camel";
char* password = "aa01200120";
String Linetoken = "您的Line 密碼"; //改為您的Line權杖密碼
// ------ 以下修改成你MQTT設定 ------
char* MQTTServer = "mqttgo.io";//免註冊MQTT伺服器
int MQTTPort = 1883;//MQTT Port
char* MQTTUser = "";//不須帳密
char* MQTTPassword = "";//不須帳密
char* MQTTPubTopic1  = "Ying/GuanghuaTaipei/Esp32Cam";//推播主題1:即時影像
long MQTTLastPublishTime;//此變數用來記錄推播時間
long MQTTPublishInterval = 3000;//每5秒推撥一次影像
WiFiClient WifiClient;
PubSubClient MQTTClient(WifiClient);

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  setupCam();
  pinMode(3, INPUT_PULLUP);//INPUT, INPUT_PULLUP,INPUT_PULLDOWN
  fInit_TFT();
  //開始網路連線
  Serial.print("連線到WiFi:");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  MQTTConnecte();

}

int i = 0; //檔案序號
void loop() {
  //如果MQTT連線中斷，則重啟MQTT連線
  if (!MQTTClient.connected()) {
    MQTTConnecte();
  }
  if ((millis() - MQTTLastPublishTime) >= MQTTPublishInterval ) {
    String result = SendImageMQTT();
    Serial.println(result);
    MQTTLastPublishTime = millis(); //更新最後傳輸時間
  }

  int v = digitalRead(3);
  //Serial.println(v);
  if (v == 0) {
    //拍照          格式         相機初始化時間(依選擇的格式修改時間)
    fTakePicture(FRAMESIZE_UXGA, 1500);
  }
  tftShow();

}


//開始MQTT連線
void MQTTConnecte() {
  MQTTClient.setServer(MQTTServer, MQTTPort);
  //MQTTClient.setCallback(MQTTCallback);
  while (!MQTTClient.connected()) {
    //以亂數為ClientID
    String  MQTTClientid = "esp32-" + String(random(1000000, 9999999));
    if (MQTTClient.connect(MQTTClientid.c_str(), MQTTUser, MQTTPassword)) {
      //連結成功，顯示「已連線」。
      Serial.println("MQTT已連線");
      //訂閱SubTopic1主題
      //MQTTClient.subscribe(MQTTSubTopic1);
    } else {
      //若連線不成功，則顯示錯誤訊息，並重新連線
      Serial.print("MQTT連線失敗,狀態碼=");
      Serial.println(MQTTClient.state());
      Serial.println("五秒後重新連線");
      delay(5000);
    }
  }
}

//拍照傳送到MQTT
String SendImageMQTT() {
  camera_fb_t * fb =  esp_camera_fb_get();
  size_t fbLen = fb->len;
  int ps = 512;
  //開始傳遞影像檔
  MQTTClient.beginPublish(MQTTPubTopic1, fbLen, false);
  uint8_t *fbBuf = fb->buf;
  for (size_t n = 0; n < fbLen; n = n + 2048) {
    if (n + 2048 < fbLen) {
      MQTTClient.write(fbBuf, 2048);
      fbBuf += 2048;
    } else if (fbLen % 2048 > 0) {
      size_t remainder = fbLen % 2048;
      MQTTClient.write(fbBuf, remainder);
    }
  }
  boolean isPublished = MQTTClient.endPublish();
  esp_camera_fb_return(fb);//清除緩衝區
  if (isPublished) {
    return "MQTT傳輸成功";
  }
  else {
    return "MQTT傳輸失敗，請檢查網路設定";
  }
}

//拍照傳送到Line
String SendImageLine(String msg, camera_fb_t * fb) {
  WiFiClientSecure client_tcp;
  if (client_tcp.connect("notify-api.line.me", 443)) {
    Serial.println("連線到Line成功");
    //組成HTTP POST表頭
    String head = "--Cusboundary\r\nContent-Disposition: form-data;";
    head += "name=\"message\"; \r\n\r\n" + msg + "\r\n";
    head += "--Cusboundary\r\nContent-Disposition: form-data; ";
    head += "name=\"imageFile\"; filename=\"esp32-cam.jpg\"";
    head += "\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Cusboundary--\r\n";
    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
    //開始POST傳送
    client_tcp.println("POST /api/notify HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: notify-api.line.me");
    client_tcp.println("Authorization: Bearer " + Linetoken);
    client_tcp.println("Content-Length: " + String(totalLen));
    client_tcp.println("Content-Type: multipart/form-data; boundary=Cusboundary");
    client_tcp.println();
    client_tcp.print(head);
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    Serial.println("傳送影像檔...");
    for (size_t n = 0; n < fbLen; n = n + 2048) {
      if (n + 2048 < fbLen) {
        client_tcp.write(fbBuf, 2048);
        fbBuf += 2048;
      } else if (fbLen % 2048 > 0) {
        size_t remainder = fbLen % 2048;
        client_tcp.write(fbBuf, remainder);
      }
    }
    client_tcp.print(tail);
    client_tcp.println();
    String payload = "";
    boolean state = false;
    int waitTime = 3000;//等候時間3秒鐘
    long startTime = millis();
    delay(1000);
    Serial.print("等候回應...");
    while ((startTime + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (client_tcp.available()) {
        //已收到回覆，依序讀取內容
        char c = client_tcp.read();
        payload += c;
      }
    }
    client_tcp.stop();
    return payload;
  }
  else {
    return "傳送失敗，請檢查網路設定";
  }
}

//鏡頭設定
void setupCam() {
  // #define CAMERA_MODEL_AI_THINKER
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;

  config.xclk_freq_hz = 10000000; //default:20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;     // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 10; //< Quality of JPEG output. 0-63 lower means higher quality
  config.fb_count = 2; //Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // sensor_t * s = esp_camera_sensor_get();
  s = esp_camera_sensor_get();

  s->set_brightness(s, 1);   //亮度 -2 to 2
  s->set_contrast(s, 1);      //對比 -2 to 2
  s->set_saturation(s, 1);    //飽和, Hue 色相
  s->set_wb_mode(s, 0);       // 0: auto 自動, 1: sun 太陽, 2: cloud 雲, 3: indoors 室內
  //s->set_exposure_ctrl(s, 1);
  //s->set_aec_value(s, -2);
  //s->set_ae_level(s, 100);
  //s->set_gain_ctrl(s, 100);
  //s->set_pixformat(s, PIXFORMAT_RGB565);
  //s->set_pixformat(s, PIXFORMAT_JPEG);
  s->set_framesize(s, FRAMESIZE_QVGA);      // 320 x 240

  Serial.println("Camera Setup OK");
}

//TFT顯示
void tftShow() {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    return;
  }

  size_t fb_len = 0;
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("Non-JPEG data not implemented");
    return;
  }

  if (pTFT_Stop == 1) pTFT_Stop = 0;

  //-------------------------------------------------------
#ifdef USE_DMA
  // Must use startWrite first so TFT chip select stays low during DMA and SPI channel settings remain configured
  tft.startWrite();
#endif

  // Draw the image, top left at 0,0 - DMA request is handled in the call-back tft_output() in this sketch
  // TJpgDec.drawJpg(0, 0, panda, sizeof(panda));
  TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
  //Serial.println(fb->len);

#ifdef USE_DMA
  // Must use endWrite to release the TFT chip select and release the SPI channel
  tft.endWrite();
#endif

OK_Err:
  esp_camera_fb_return(fb);
}

//拍照存檔
void fTakePicture(framesize_t fs, int t) {
  pTFT_Stop = 1;
  s->set_framesize(s, fs);
  delay(t);//保留切換時間，避免進光量不足

  Serial.println("Taking Picture");
  camera_fb_t *fb_P = NULL;
  esp_err_t res = ESP_OK;

  // Take Picture with Camera
  fb_P = esp_camera_fb_get();
  if (!fb_P) {
    Serial.println("Camera capture failed - TakePicture");
    esp_camera_fb_return(fb_P);
    fb_P = NULL;
    return;
  }

  //-------------------------------------------
  vInitSD_OK = 1;

  Serial.println("Saving to TFCard");

  //初始化記憶卡
  if (!SD_MMC.begin()) {
    Serial.println("TF卡讀取失敗");
    //return;
    vInitSD_OK = 0;
  }
  else {
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("找不到記憶卡");
      //return;
      vInitSD_OK = 0;
    }
  }

  //拍照存檔
  if (vInitSD_OK == 1) {
    // initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1;

    // Path where new picture will be saved in SD Card
    String path = "/picture" + String(pictureNumber) + ".jpg";

    fs::FS &fs = SD_MMC;
    Serial.printf("Picture file name: %s\n", path.c_str());

    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
      Serial.println("記憶卡無法寫入");
    }
    else {
      file.write(fb_P->buf, fb_P->len);   // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path.c_str());

      EEPROM.write(0, pictureNumber);
      EEPROM.commit();
    }
    file.close();//關閉檔案
  }

  SD_MMC.end();//關閉TF卡
  SPI.end();//關閉SPI

  //SPI.begin(sck, miso, mosi, ss)
  SPI.begin(14, 15, 12, -1);   //ESP32-CAM TFT- 111.07.25-搞定

  //------------------------------------------------
  Serial.println("重新初始化tft腳位");
  tft.ReInit_Pin();//-111.07,.24-CGH，重新定義腳位
  tft.init();//重新初始化tft

  //------------------------------------------
  esp_camera_fb_return(fb_P);
  fb_P = NULL;
  s->set_framesize(s, FRAMESIZE_QVGA);//改為TFT解析度
  delay(500);
}

//初始化TFT
void fInit_TFT() {
  tft.init();    // Init ST7789 display 240x240 pixel
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);     //0:-0 1:landscape-90 2:-180 3:inv. landscape-270

#ifdef USE_DMA
  tft.initDMA();        // To use SPI DMA you must call initDMA() to setup the DMA engine
#endif

  // The jpeg image can be scaled down by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The colour byte order can be swapped by the decoder
  // using TJpgDec.setSwapBytes(true); or by the TFT_eSPI library:
  tft.setSwapBytes(true);

  //#ifdef USE_DMA
  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);
  //#endif
}

//TFT輸出
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (pTFT_Stop == 1) return 0;

  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) return 0;

  // STM32F767 processor takes 43ms just to decode (and not draw) jpeg (-Os compile option)
  // Total time to decode and also draw to TFT:
  // SPI 54MHz=71ms, with DMA 50ms, 71-43 = 28ms spent drawing, so DMA is complete before next MCU block is ready
  // Apparent performance benefit of DMA = 71/50 = 42%, 50 - 43 = 7ms lost elsewhere
  // SPI 27MHz=95ms, with DMA 52ms. 95-43 = 52ms spent drawing, so DMA is *just* complete before next MCU block is ready!
  // Apparent performance benefit of DMA = 95/52 = 83%, 52 - 43 = 9ms lost elsewhere
#ifdef USE_DMA
  // Double buffering is used, the bitmap is copied to the buffer by pushImageDMA() the
  // bitmap can then be updated by the jpeg decoder while DMA is in progress
  if (dmaBufferSel)
    dmaBufferPtr = dmaBuffer2;
  else
    dmaBufferPtr = dmaBuffer1;

  dmaBufferSel = !dmaBufferSel; // Toggle buffer selection

  //  pushImageDMA() will clip the image block at screen boundaries before initiating DMA
  tft.pushImageDMA(x, y, w, h, bitmap, dmaBufferPtr); // Initiate DMA - blocking only if last DMA is not complete
  // The DMA transfer of image block to the TFT is now in progress...
#else
  // Non-DMA blocking alternative
  tft.pushImage(x, y, w, h, bitmap);  // Blocking, so only returns when image block is drawn
#endif

  // Return 1 to decode next block.
  return 1;
}
