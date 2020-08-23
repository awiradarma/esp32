#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <U8g2lib.h>
#include <cQueue.h>

#define   FONT_ONE_HEIGHT               7                                   // font one height in pixels
#define   FONT_TWO_HEIGHT               20                                  // font two height in pixels
#define	  IMPLEMENTATION	              FIFO
#define   FORWARD                       1
#define   BACKWARD                      2
#define   LEFT                          3
#define   RIGHT                         4
#define   STOP                          5
U8G2_SSD1306_128X64_NONAME_F_HW_I2C       u8g2(U8G2_R0, 16, 15, 4);

const char* ssid = "ssid";
const char* password = "xxxx";
char      chBuffer[128];
Queue_t		q;

WiFiClient cl;

WebServer server(80);

const int led = 25;
int count=0;

TaskHandle_t Task1;

typedef struct dc_motor {
  int on_pin; // pin to turn on / off the DC motor
  int reverse_pin; // pin to reverse direction of the rotation
} Motor;

Motor A;
Motor B;

typedef struct strRec {
	int	command;
	int	duration;
} Rec;

void handleRoot() {
  digitalWrite(led, 1);
  // cl = server.client();
  char  chIp[81];
  Serial.print("Client IP: ");
  Serial.println(server.client().remoteIP());
  server.client().remoteIP().toString().toCharArray(chIp, sizeof(chIp) - 1);
  sprintf(chBuffer, "Client: %s", chIp);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 4, chBuffer);
  u8g2.sendBuffer();

  updateCount();
  
  server.send(200, "text/plain", "hello from esp32!");
  
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setPinMode(void) {
  for (uint8_t i = 12; i < 34; i++) {
    pinMode(i, OUTPUT);
  }
}

void setup(void) {
  
  setPinMode();
  A.on_pin = 12;
  A.reverse_pin = 13;
  B.on_pin = 26;
  B.reverse_pin = 27;

  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  u8g2.begin();
  //u8g2.setFont(u8g2_font_7x13_mr);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  char  chIp[81];
  WiFi.localIP().toString().toCharArray(chIp, sizeof(chIp) - 1);
  sprintf(chBuffer, "IP : %s", chIp);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 2, chBuffer);
  
  // Display the ssid of the wifi router.
      
  sprintf(chBuffer, "SSID : %s", ssid);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 3, chBuffer);
  u8g2.sendBuffer();

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/on/{}", [] () {
    String pin = server.pathArg(0);
    digitalWrite(pin.toInt(),HIGH);
    server.send(200, "text/plain", "turning ON : " + pin);
  });

  server.on("/off/{}", [] () {
    String pin = server.pathArg(0);
    digitalWrite(pin.toInt(),LOW);
    server.send(200, "text/plain", "turning OFF : " + pin);
  });

  server.on("/move/{}", []() {
    String duration = "500"; // default duration for each move command is 500 ms
    String command = server.pathArg(0);

    digitalWrite(led, 1);
    //updateCount();

    if (server.hasArg("duration")) {
      duration = server.arg("duration");
    }
    server.send(200, "text/plain", "command: " + command + " ( " + duration + " ms )");

    if (command.equalsIgnoreCase("forward")) pushToQueue(FORWARD, duration.toInt());
    if (command.equalsIgnoreCase("backward")) pushToQueue(BACKWARD, duration.toInt());
    if (command.equalsIgnoreCase("left")) pushToQueue(LEFT, duration.toInt());
    if (command.equalsIgnoreCase("right")) pushToQueue(RIGHT, duration.toInt());
    if (command.equalsIgnoreCase("stop")) pushToQueue(STOP, duration.toInt());
    
    digitalWrite(led, 0);
    Serial.print("Got a request to move " + command + " for " + duration);
    Serial.print(" - running on core ");
    Serial.println(xPortGetCoreID());
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  q_init(&q, sizeof(Rec), 100, IMPLEMENTATION, false);

  // Start a separate task to check for the queue, running on core 0
  xTaskCreatePinnedToCore(
      checkQueue, /* Function to implement the task */
      "checkQueue", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      5,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */

}

void updateCount() {
  count+=1;
  sprintf(chBuffer, "# of hits: %d", count);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 5, chBuffer);
  u8g2.sendBuffer();
}

void showCurrent(String command) {
  char chbuf[25];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tr);
  command.toCharArray(chbuf, sizeof(chbuf) - 1);
  sprintf(chBuffer, " %s ", chbuf);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 5, chBuffer);
  u8g2.sendBuffer();
}

void pushToQueue(int i, int duration) {
  Rec rec = {i, duration};
  q_push(&q, &rec);
}

void loop(void) {
  server.handleClient();
  yield();
}

void checkQueue (void * parameter) {
  
  int processing = 0;
  unsigned long timeToStop;
  Rec rec;

  for(;;) {
    
    if (processing == 0) {
      if (q_getCount(&q) > 0) {
        q_pop(&q, &rec);
        Serial.print("Processing item from queue - running on core ");
        Serial.print(xPortGetCoreID());
        Serial.print(" ");
        Serial.print(rec.command);
        Serial.print(" : ");
        Serial.println(rec.duration);
        if (rec.command == FORWARD) { processing = 1; timeToStop = millis() + rec.duration; showCurrent("FORWARD"); forward(); }
        if (rec.command == BACKWARD) { processing = 1; timeToStop = millis() + rec.duration; showCurrent("BACKWARD"); backward(); }
        if (rec.command == LEFT) { processing = 1; timeToStop = millis() + rec.duration; showCurrent("LEFT"); left(); }
        if (rec.command == RIGHT) { processing = 1; timeToStop = millis() + rec.duration; showCurrent("RIGHT"); right(); }
        if (rec.command == STOP) { processing = 1; timeToStop = millis() + rec.duration; showCurrent("STOP"); stopMotors(); }
      } 
    } else {
        if (millis() > timeToStop) {
          processing=0;
          stopMotors();
          showCurrent("STOPPED");
        }
    }
    delay(10);
    yield();
  }
}
void stopMotors(void) {
  digitalWrite(A.on_pin, LOW);
  digitalWrite(B.on_pin, LOW);
  digitalWrite(A.reverse_pin, LOW);
  digitalWrite(B.reverse_pin, LOW);
}

void forward(void) { 
  digitalWrite(A.reverse_pin, LOW);
  digitalWrite(B.reverse_pin, LOW);
  digitalWrite(A.on_pin, HIGH);
  digitalWrite(B.on_pin, HIGH);
}

void backward(void) { 
  digitalWrite(A.reverse_pin, HIGH);
  digitalWrite(B.reverse_pin, HIGH);
  digitalWrite(A.on_pin, LOW);
  digitalWrite(B.on_pin, LOW);
}

void left(void) { 
  digitalWrite(A.reverse_pin, LOW);
  digitalWrite(B.reverse_pin, HIGH);
  digitalWrite(A.on_pin, HIGH);
  digitalWrite(B.on_pin, LOW);
}


void right(void) { 
  digitalWrite(A.reverse_pin, HIGH);
  digitalWrite(B.reverse_pin, LOW);
  digitalWrite(A.on_pin, LOW);
  digitalWrite(B.on_pin, HIGH);
}
