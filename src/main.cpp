#include <Arduino.h>
#include <ClickEncoder.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <Rotary.h>
#include <Button2.h>
#include <pcm5121.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <ESP8266httpUpdate.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <IRutils.h>
#include <cs8422.h>
#define MAX_VOLUME 100
#define MAX_MENU 9
#define LED_BUILD 2
#define ENCODER_PINA 13
#define ENCODER_PINB 12
#define ENCODER_BTN 0
#define IR_RCV_PIN 14
#define CS8422_INT_PIN 9
#define CS8422_RST_PIN 10
#define ENCODER_STEPS_PER_NOTCH 4 // Change this depending on which encoder is used
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

const char *ssid = "shengji3";
const char *password = "sonavox168";
static WiFiEventHandler e1, e2, e3;
WiFiClient espClient;
AsyncWebServer server(80);
String mac = "";
Rotary encoder = Rotary(12, 13);
//full framebuffer, size = 1024 bytes  6 pages * 128 bytes
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
Button2 push_button = Button2(ENCODER_BTN);
//IRrecv irrecv(IR_RCV_PIN);
const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;
IRrecv irrecv(IR_RCV_PIN, kCaptureBufferSize, kTimeout, true);
decode_results results;
uint8_t master_volume = 50;
// uint8_t trebel_volume = 50;
// uint8_t middle_volume = 50;
// uint8_t bass_volume = 50;
uint8_t page_index = 0;
uint8_t input_index = 0;
uint8_t last_index = 0;
String menu[] = {"Optical-1", "Optical-2", "Coaxial-1", "Coaxial-2", "I2S IN", "SmartConfig", "Device Info", "About", "Exit"};
void onSTAConnected(WiFiEventStationModeConnected ipInfo)
{
  DEBUG_PRINT("Connected to %s\r\n", ipInfo.ssid.c_str());
}

// Start NTP only after IP network is connected
void onSTAGotIP(WiFiEventStationModeGotIP ipInfo)
{
  DEBUG_PRINT("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
  DEBUG_PRINT("Connected: %s\r\n", WiFi.status() == WL_CONNECTED ? "yes" : "no");
  digitalWrite(LED_BUILD, LOW); // Turn on LED
}

// Manage network disconnection
void onSTADisconnected(WiFiEventStationModeDisconnected event_info)
{
  DEBUG_PRINT("Disconnected from SSID: %s\n", event_info.ssid.c_str());
  DEBUG_PRINT("Reason: %d\n", event_info.reason);
  digitalWrite(LED_BUILD, HIGH);
}
//长按3秒种进入配网模式
void smart_config()
{
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  //Wait for SmartConfig packet from mobile
  DEBUG_PRINT("\r\nWaiting for SmartConfig.");
  while (!WiFi.smartConfigDone())
  {
    delay(200);
    //Serial.print(".");
    digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
  }
  DEBUG_PRINT("SmartConfig Received.");
  //Wait for WiFi to connect to AP
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(600);
    Serial.print(".");
    digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
  }
  DEBUG_PRINT("\r\nWiFi Connected.");
  //DEBUG_PRINT("ip:%s", WiFi.localIP());
  digitalWrite(LED_BUILD, 0);
  // delay(1000);
  //DEBUG_PRINT("Reset CPU");
  //ESP.restart();
}
void update()
{
  ESPhttpUpdate.setLedPin(LED_BUILD, LOW);
  t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, "http://192.168.1.8/barcode.bin");
  //t_httpUpdate_return  ret = ESPhttpUpdate.update("https://server/file.bin", "", "fingerprint");

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    DEBUG_PRINT("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    DEBUG_PRINT("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    DEBUG_PRINT("HTTP_UPDATE_OK");
    break;
  }
}
void draw_page(uint8_t index)
{
  switch (index)
  {
  case 0: // show main page
  {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 14);
    u8g2.print(menu[input_index]);
    u8g2.setCursor(86, 14);
    u8g2.print(get_receiver_status() & 0x10 ? "locked" : "nolock");
    u8g2.drawHLine(0, 18, 128);
    u8g2.setCursor(32, 40);
    u8g2.setFont(u8g2_font_10x20_mr);
    if (master_volume == 0)
    {
      u8g2.print("Mute");
    }
    else
    {
      u8g2.print("VOL ");
      u8g2.print(master_volume);
    }
    u8g2.setFont(u8g2_font_open_iconic_play_1x_t);
    //u8g2.setCursor(0, 60);
    //u8g2.print("0");
    u8g2.drawGlyph(3, 60, 0x0050);
    u8g2.drawBox(14, 50, master_volume, 12);
    //u8g2.setCursor(114, 60);
    //u8g2.print("100");
    u8g2.drawGlyph(120, 60, 0x004F);
    u8g2.sendBuffer();
    u8g2.setFont(u8g2_font_7x14_mr);
    set_pcm5121_volume(master_volume, master_volume);
  }
  break;
  case 1: //show menu page
  {
    u8g2.clearBuffer();

    if (input_index >= 4)
    {
      if (input_index > MAX_MENU)
      {
        input_index = MAX_MENU;
      }
      for (uint8_t i = 0; i < 4; i++)
      {
        u8g2.setFontMode(1);
        u8g2.setDrawColor(2); //font mode 1 and transparent	2	    XOR
        u8g2.setCursor(32, 14 * (i + 1));
        u8g2.print(menu[i + 1 + input_index - 4]);
      }
      u8g2.drawBox(0, 14 * 3 + 2, 128, 14);
    }
    else
    {
      for (uint8_t i = 0; i < 4; i++)
      {
        if (input_index == i)
        {
          u8g2.drawBox(0, (14 * i) + 2, 128, 14);
        }
        u8g2.setFontMode(1);
        u8g2.setDrawColor(2); //font mode 1 and transparent	2	    XOR
        u8g2.setCursor(32, 14 * (i + 1));
        u8g2.print(menu[i]);
      }
    }
    u8g2.sendBuffer();
    break;
  }
  case 2: // show smartconfig page
  {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 40);
    u8g2.print("Wifi Smartconfig...");
    u8g2.sendBuffer();
  }
  break;
  case 3: // show DeviceInfo page
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_12_mr);
    u8g2.setCursor(0, 10);
    u8g2.print("IP:");
    u8g2.setCursor(0, 20);
    u8g2.print(WiFi.localIP().toString());
    u8g2.setCursor(0, 30);
    u8g2.print("MAC:");
    u8g2.setCursor(0, 40);
    u8g2.print(WiFi.macAddress());
    u8g2.setCursor(0, 50);
    u8g2.print("SSID:");
    u8g2.setCursor(0, 60);
    u8g2.print(WiFi.SSID());
    u8g2.sendBuffer();
    u8g2.setFont(u8g2_font_7x14_mr);
  }
  break;
  case 4: // show DeviceInfo page
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_12_mr);
    u8g2.setCursor(46, 14);
    u8g2.printf("About");
    u8g2.setCursor(0, 28);
    u8g2.printf("code ver:%s", "1.0.0");
    u8g2.setCursor(0, 42);
    u8g2.printf("webpage ver:%s", "1.0.0");
    u8g2.setCursor(0, 56);
    u8g2.printf("%s %s", __DATE__, __TIME__);
    u8g2.sendBuffer();
    u8g2.setFont(u8g2_font_7x14_mr);
  }
  break;
  }
}
uint8_t checkRange(uint8_t max_value, uint8_t dat)
{
  if (dat >= max_value)
  {
    dat = max_value - 1;
  }
  else if (dat <= 0)
  {
    dat = 1;
  }
  return dat;
}

void handler_key(Button2 &btn)
{
  switch (btn.getClickType())
  {
  case SINGLE_CLICK:
    page_index++;
    if (page_index == 2) //按一下进入菜单，再按一下退出菜单
    {
      if (input_index <= 4) //前边5路信号源
      {
        page_index = 0; //Main Page
        last_index = input_index;
        select_input(last_index);
      }
      else if (input_index == 5)
      {
        page_index = 2;
      }
      else if (input_index == 6)
      {
        page_index = 3;
      }
      else if (input_index == 7)
      {
        page_index = 4;
      }
      else if (input_index == 8)
      {
        page_index = 0;
        input_index = last_index;
      }
    }
    else if (page_index >= 3) //再按一下返回
    {
      page_index = 1;
    }
    break;
  case DOUBLE_CLICK:
    Serial.print("double ");
    break;
  case TRIPLE_CLICK:
    Serial.print("triple ");
    break;
  case LONG_CLICK:

    Serial.print("-----long click------\n");
    smart_config();
    break;
  }
  draw_page(page_index);
}
void IntCallback()
{
  printf("CS8422 Format Status:0x%X\n", get_format_status());
  printf("CS8422 PLL Status:0x%X\n", get_pll_status());
  printf("CS8422 Receiver Status:0x%X\n", get_receiver_status());
  printf("CS8422 Receiver Error:0x%X\r\n", get_error_status());
  printf("CS8422 Interrupt Status:0x%X\r\n", get_interrupt_status());
  if (page_index == 0)
  {
    u8g2.setCursor(86, 14);
    u8g2.print(get_receiver_status() & 0x10 ? "locked" : "nolock");
    u8g2.sendBuffer();
  }
}
void setup()
{
  Serial.begin(115200);
  pinMode(CS8422_INT_PIN, INPUT_PULLUP);
  pinMode(CS8422_RST_PIN, OUTPUT);
  pinMode(LED_BUILD, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(15, OUTPUT);
  digitalWrite(16, 1);
  digitalWrite(15, 1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  long lastMsg = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    Serial.print(".");
    digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
    //这里如果没有网络会陷入死循环
    if (millis() - lastMsg > 10000)
    {
      break;
    }
  }
  //WiFi.begin();
  Serial.print("\nConnecting WiFi...\n");
  mac = WiFi.macAddress();
  //mac.replace(":", "");                //去掉：号
  mac.toLowerCase(); //转为小写
  DEBUG_PRINT("Mac Address:%s\n", mac.c_str());
  DEBUG_PRINT("Compiled DateTime: %s %s\n", __DATE__, __TIME__);
  //irrecv.enableIRIn(); // Start the receiver
  //https://www.jianshu.com/p/014bcae94c8b

  bool ok = SPIFFS.begin();
  if (ok)
  {
    DEBUG_PRINT("Spiffs Mount Ok\r\n");
    //SPIFFS.format();
    //SPIFFS 1M
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      File f = dir.openFile("r");
      DEBUG_PRINT("File Name:%s File Size:%d\r\n", dir.fileName().c_str(), f.size());
    }
  }
  else
  {
    DEBUG_PRINT("Spiffs Mount Failed\r\n");
  }

  u8g2.begin();
  u8g2.enableUTF8Print(); // enable UTF8 support for the Arduino print() function
  u8g2.clearBuffer();
  //u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setFont(u8g2_font_7x14_mr);
  u8g2.setFontDirection(0);
  draw_page(0);
  push_button.setClickHandler(handler_key);
  push_button.setLongClickHandler(handler_key);
  attachInterrupt(digitalPinToInterrupt(CS8422_INT_PIN), IntCallback, RISING);
  irrecv.enableIRIn(); // Start the receiver
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setAuthentication("admin", "admin");
  //server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setAuthentication("admin", "admin");

  //server.serveStatic("/js", SPIFFS, "/js");
  //server.serveStatic("/style", SPIFFS, "/style");
  //server.serveStatic("/fonts", SPIFFS, "/fonts");
  server.onNotFound([](AsyncWebServerRequest *request) {
    DEBUG_PRINT("NOT_FOUND: ");
    DEBUG_PRINT(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength())
    {
      DEBUG_PRINT("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      DEBUG_PRINT("_CONTENT_LENGTH: %u\n", request->contentLength());
    }
    int headers = request->headers();
    for (int i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      DEBUG_PRINT("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
    request->send(404);
  });

  server.on("/get_info", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    //int index = p->value().toInt();
    root["heap"] = ESP.getFreeHeap();
    root["ssid"] = WiFi.SSID();
    root["mode"] = "DHCP";
    root["ip"] = WiFi.localIP().toString();
    root["gateway"] = WiFi.gatewayIP().toString();
    root["dns"] = WiFi.dnsIP().toString();
    root["mask"] = WiFi.subnetMask().toString();
    root["time"] = "time";
    root["server"] = "ntp1.aliyun.com";
    response->setLength();
    request->send(response);
  });
  // server.on("/info.html", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   //String amp_name = request->getParam(0)->value();
  //   request->send(SPIFFS, "/burn.html");
  // });

  server.on("/get_time", HTTP_POST, [](AsyncWebServerRequest *request) {
    //int params = request->params();
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    root["rtc_time"] = "2019/05/30 12：00：00";
    root["temp"] = "20度";
    response->setLength();
    request->send(response);
  });
  server.begin();

  pcm5121_init();
  set_pcm5121_volume(50, 50);
  digitalWrite(CS8422_RST_PIN, 0);
  delay(100);
  digitalWrite(CS8422_RST_PIN, 1);
  delay(100);
  cs8422_init();
  printf("CS8422 Chip ID:0x%X\n", get_cs8422_id());
}
void loop()
{
  unsigned char dir = encoder.process();
  if (dir) //DIR_NONE
  {

    if (page_index == 0)
    {
      master_volume = checkRange(MAX_VOLUME, master_volume);
      dir == DIR_CW ? master_volume++ : master_volume--;
      draw_page(page_index);
    }
    else if (page_index == 1)
    {
      if (dir == DIR_CW)
      {
        input_index++;
        if (input_index > MAX_MENU - 1)
        {
          input_index = MAX_MENU - 1;
        }
      }
      else if (dir == DIR_CCW)
      {
        if (input_index <= 0)
        {
          input_index = 1;
        }
        input_index--;
      }
      //DEBUG_PRINT("input_index:%d\r\n", input_index);

      draw_page(page_index);
    }
  }
  push_button.loop();
  if (irrecv.decode(&results))
  {
    if (results.repeat == true)
    {
    }
    //DEBUG_PRINT(resultToHumanReadableBasic(&results).c_str());
    DEBUG_PRINT(resultToSourceCode(&results).c_str());
    //"%02X"，是以0补齐2位数，如果超过2位就显示实际的数，字母数值大写，如果换为x，字母数值就小写。
    DEBUG_PRINT("Address:0x%04X Command:0x%02X\n", results.address, results.command);
  }
}
