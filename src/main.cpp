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
#define MAX_MENU 10
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
AsyncWebSocket ws("/ws");           // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)
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
//遥控器代码定义
uint8_t command = 0;
uint8_t last_command = 0;
uint32_t remote_code = 0x0000;
uint8_t volume_up_code = 0x46;
uint8_t volume_down_code = 0x16;
uint8_t volume_mute_code = 0x06;
uint8_t power_code = 0x18;
uint8_t source_code = 0x42;
uint8_t bt_pre_code = 0x47;
uint8_t bt_next_code = 0x15;
uint8_t bt_pause_code = 0x55;
uint8_t opt1_code = 0x14;
uint8_t opt2_code = 0x10;
uint8_t coax1_code = 0x17;
uint8_t coax2_code = 0x40;
uint8_t bt_code = 0x04;
uint8_t mute_code = 0x04;
String menu[] = {"Optical-1", "Optical-2", "Coaxial-1", "Coaxial-2", "I2S IN", "SmartConfig", "IR Decode", "Device Info", "About", "Exit"};
DynamicJsonBuffer jsonBuffer;
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
    break;
  case 1: //show menu page
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
  case 2: // show smartconfig page
    u8g2.clearBuffer();
    u8g2.setCursor(0, 40);
    u8g2.print("Wifi Smartconfig...");
    u8g2.sendBuffer();
    break;
  case 3: // show IR Decode
    u8g2.clearBuffer();
    u8g2.setCursor(30, 14);
    u8g2.print("IR Decoder");
    u8g2.setCursor(0, 28);
    u8g2.print("Address:");
    u8g2.setCursor(56, 28);
    u8g2.print("0x00");
    u8g2.setCursor(0, 42);
    u8g2.print("command:");
    u8g2.setCursor(56, 42);
    u8g2.print("0x00");
    u8g2.sendBuffer();
    break;
  case 4: // show DeviceInfo page
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
    break;
  case 5: // show DeviceInfo page
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
        page_index = 5;
      }
      else if (input_index == 9)
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
  // printf("CS8422 Format Status:0x%X\n", get_format_status());
  // printf("CS8422 PLL Status:0x%X\n", get_pll_status());
  // printf("CS8422 Receiver Status:0x%X\n", get_receiver_status());
  // printf("CS8422 Receiver Error:0x%X\r\n", get_error_status());
  // printf("CS8422 Interrupt Status:0x%X\r\n", get_interrupt_status());
  if (page_index == 0)
  {
    draw_page(page_index);
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    //client connected
    os_printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    //client disconnected
    os_printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    //error was received from the other end
    os_printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    //pong message was received (in response to a ping request maybe)
    os_printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    //data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      os_printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        os_printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < info->len; i++)
        {
          os_printf("%02x ", data[i]);
        }
        os_printf("\n");
      }
      if (info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          os_printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        os_printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      os_printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        os_printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          os_printf("%02x ", data[i]);
        }
        os_printf("\n");
      }

      if ((info->index + len) == info->len)
      {
        os_printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          os_printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
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
  //https://github.com/pellepl/spiffs/wiki/Using-spiffs
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
    //模式为w时，是不能读取的，必须分开处理
    File f = SPIFFS.open("/remote_code.cfg", "r");
    if (!f)
    {
      DEBUG_PRINT("Remote Code Config Is Not Exists\r\n");
    }
    else
    {
      String json = "";
      while (f.available())
      {
        json += char(f.read());
      }
      DEBUG_PRINT("Remote Data:%s\r\n", json.c_str());
      JsonObject &root = jsonBuffer.parseObject(json);
      if (!root.success())
      {
        f.close();
        DEBUG_PRINT("Remote Code Config Data parse failed.\r\n");
        JsonObject &root = jsonBuffer.createObject();
        root["remote_code"] = remote_code;
        root["volume_up_code"] = volume_up_code;
        root["volume_down_code"] = volume_down_code;
        root["volume_mute_code"] = volume_mute_code;
        root["bt_next_code"] = bt_next_code;
        root["bt_pre_code"] = bt_pre_code;
        root["bt_pause_code"] = bt_pause_code;
        root["bt_code"] = bt_code;
        root["mute_code"] = mute_code;
        root["power_code"] = power_code;
        root["source_code"] = source_code;
        root["opt1_code"] = opt1_code;
        root["opt2_code"] = opt2_code;
        root["coax1_code"] = coax1_code;
        root["coax2_code"] = coax2_code;
        File ff = SPIFFS.open("/remote_code.cfg", "w");
        String s = "";
        root.prettyPrintTo(s);
        int bytesWritten = ff.print(s);
        if (bytesWritten > 0)
        {
          DEBUG_PRINT("File was written\r\n");
        }
        else
        {
          DEBUG_PRINT("File write failed\r\n");
        }
        ff.close();
      }
      else
      {
        remote_code = root["remote_code"];
        volume_up_code = root["volume_up_code"];
        volume_down_code = root["volume_down_code"];
        volume_mute_code = root["volume_mute_code"];
        bt_next_code = root["bt_next_code"];
        bt_pre_code = root["bt_pre_code"];
        bt_pause_code = root["bt_pause_code"];
        bt_code = root["bt_code"];
        mute_code = root["mute_code"];
        power_code = root["power_code"];
        source_code = root["source_code"];
        opt1_code = root["opt1_code"];
        opt2_code = root["opt2_code"];
        coax1_code = root["coax1_code"];
        coax2_code = root["coax2_code"];
      }
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
    root["mode"] = "dhcp";
    root["ip"] = WiFi.localIP().toString();
    root["gateway"] = WiFi.gatewayIP().toString();
    root["dns"] = WiFi.dnsIP().toString();
    root["mask"] = WiFi.subnetMask().toString();
    root["rtc_time"] = "time";
    root["ntp_server"] = "ntp1.aliyun.com";
    response->setLength();
    request->send(response);
  });
  server.on("/get_remote_cfg", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    root["remote_code"] = remote_code;
    root["volume_up_code"] = volume_up_code;
    root["volume_down_code"] = volume_down_code;
    root["volume_mute_code"] = volume_mute_code;
    root["bt_next_code"] = bt_next_code;
    root["bt_pre_code"] = bt_pre_code;
    root["bt_pause_code"] = bt_pause_code;
    root["bt_code"] = bt_code;
    root["mute_code"] = mute_code;
    root["power_code"] = power_code;
    root["source_code"] = source_code;
    root["opt1_code"] = opt1_code;
    root["opt2_code"] = opt2_code;
    root["coax1_code"] = coax1_code;
    root["coax2_code"] = coax2_code;
    response->setLength();
    request->send(response);
  });
  server.on("/set_remote_cfg", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    //保存参数
    File f = SPIFFS.open("/remote_code.cfg", "w");
    f.print(p->value());
    f.close();
    remote_code = root["remote_code"];
    volume_up_code = root["volume_up_code"];
    volume_down_code = root["volume_down_code"];
    volume_mute_code = root["volume_mute_code"];
    bt_next_code = root["bt_next_code"];
    bt_pre_code = root["bt_pre_code"];
    bt_pause_code = root["bt_pause_code"];
    bt_code = root["bt_code"];
    mute_code = root["mute_code"];
    power_code = root["power_code"];
    source_code = root["source_code"];
    opt1_code = root["opt1_code"];
    opt2_code = root["opt2_code"];
    coax1_code = root["coax1_code"];
    coax2_code = root["coax2_code"];
    rsp["status"] = "success";
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
  server.on("/get_dac_status", HTTP_POST, [](AsyncWebServerRequest *request) {
    //int params = request->params();
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    root["source"] = last_index;
    root["volume"] = master_volume;
    response->setLength();
    request->send(response);
  });
  server.on("/set_volume", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    master_volume = root["volume"];
    rsp["status"] = "success";
    response->setLength();
    request->send(response);
    draw_page(page_index);
  });
  server.on("/set_source", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    input_index = root["source"];
    last_index = input_index;
    select_input(last_index);
    rsp["status"] = "success";
    response->setLength();
    request->send(response);
    draw_page(page_index);
  });

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // attach AsyncEventSource
  server.addHandler(&events);
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

    if (input_index == 6)
    {
      u8g2.clearBuffer();
      u8g2.setCursor(30, 14);
      u8g2.print("IR Decoder");
      u8g2.setCursor(0, 28);
      u8g2.printf("Address:0x%04X", results.address);
      u8g2.setCursor(0, 42);
      u8g2.printf("command:0x%02X", results.command);
      u8g2.sendBuffer();
      //DEBUG_PRINT(resultToHumanReadableBasic(&results).c_str());
      DEBUG_PRINT(resultToSourceCode(&results).c_str());
      //"%02X"，是以0补齐2位数，如果超过2位就显示实际的数，字母数值大写，如果换为x，字母数值就小写。
      DEBUG_PRINT("Address:0x%04X Command:0x%02X\n", results.address, results.command);
      return;
    }
    command = results.command;
    if (command != 0x00)
    {
      last_command = command;
    }
    DEBUG_PRINT("Command:0x%02X\n", command);
    if (remote_code != results.address)
    {
      return;
    }
    if (results.repeat == true && last_command == volume_up_code)
    {
      master_volume = checkRange(MAX_VOLUME, master_volume);
      master_volume++;
      delay(100);
    }
    else if (results.repeat == true && last_command == volume_down_code)
    {
      master_volume = checkRange(MAX_VOLUME, master_volume);
      master_volume--;
      delay(100);
    }
    else if (command == volume_up_code)
    {
      master_volume = checkRange(MAX_VOLUME, master_volume);
      master_volume++;
    }
    else if (command == volume_down_code)
    {
      master_volume = checkRange(MAX_VOLUME, master_volume);
      master_volume--;
    }
    else if (command == volume_mute_code)
    {
    }
    else if (command == bt_next_code)
    {
    }
    else if (command == bt_pre_code)
    {
    }
    else if (command == bt_pause_code)
    {
    }
    else if (command == source_code)
    {
      input_index++;
      if (input_index >= 5)
      {
        input_index = 0;
      }
      last_index = input_index;
      select_input(last_index);
    }
    else if (command == opt1_code)
    {
      input_index = 0;
    }
    else if (command == opt2_code)
    {
      input_index = 1;
    }
    else if (command == coax1_code)
    {
      input_index = 2;
    }
    else if (command == coax2_code)
    {
      input_index = 3;
    }
    else if (command == power_code)
    {
    }
    else if (command == volume_up_code)
    {
    }
    else
    {
    }
    draw_page(page_index);
  }
}
