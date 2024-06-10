/*********
  Rui Santos
  참조 사이트 - https://randomnerdtutorials.com/esp8266-web-server/
  CP210x USB to UART Bridge 드라이버 - https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads
*********/

// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <WebSocketsServer.h>
#include <FS.h>

// Replace with your network credentials
const char* ssid     = "";
const char* password = "";

// Set web server
WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
String header;

// sensor data
float temperature = 0.0;
float humidity = 0.0;

const int dataPoints = 10;
float temperatureData[dataPoints];
float humidityData[dataPoints];
int dataIndex = 0;

unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

byte board13[] = {0xcc,0x50,0xe3,0xce,0x0b,0xe0};
byte board10[] = {0xcc,0x50,0xe3,0xcd,0x96,0xed};

unsigned long t = 0;

struct SensorData {
  float temperature;
  float humidity;
};

SensorData dataToSend;

void setup() {
  Serial.begin(9600);

  // Initialize the SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  WiFi.mode(WIFI_STA); //set wifi mode(station mode)
  
  // Connect to Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //ESPNOW on
  if (esp_now_init() != 0) {
    Serial.println("ESPNOW 실패!");
    return;
  }else{
     Serial.println("ESPNOW 성공!");
  }

  //board role(Two-way)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);


  esp_now_add_peer(board10, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  //send,recv callback
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  webSocket.loop();
  WiFiClient client = server.available();

  //Implementing web pages
  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();         
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Data download
            if (header.indexOf("GET /download") >= 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/plain");
              client.println("Content-Disposition: attachment; filename=data.txt");
              client.println("Connection: close");
              client.println();

              // Send the data
              for (int i = 0; i < dataPoints; i++) {
                client.println("Temperature: " + String(temperatureData[i]) + " °C, Humidity: " + String(humidityData[i]) + " %");
              }

              client.stop();
              return;
            }

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=UTF-8");
            client.println("Connection: close");
            client.println();
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>");
            client.println("<script>var connection = new WebSocket('ws://' + location.hostname + ':81/');");
            client.println("connection.onmessage = function (event) {");
            client.println("var data = JSON.parse(event.data);");
            client.println("document.getElementById('temperature').innerHTML = data.temperature + ' &deg;C';");
            client.println("document.getElementById('humidity').innerHTML = data.humidity + ' %';");
            client.println("updateChart(temperatureChart, data.temperature);");
            client.println("updateChart(humidityChart, data.humidity);");
            client.println("if (data.temperature > 31) { showPopup('Very Warning', 'red'); }");
            client.println("else if (data.temperature > 30) { showPopup('Warning', 'orange'); }");
            client.println("};");
            client.println("function updateChart(chart, value) {");
            client.println("chart.data.labels.push('');");
            client.println("chart.data.datasets[0].data.push(value);");
            client.println("if (chart.data.labels.length > " + String(dataPoints) + ") {");
            client.println("chart.data.labels.shift();");
            client.println("chart.data.datasets[0].data.shift();");
            client.println("}");
            client.println("chart.update();");
            client.println("}");
            client.println("function showPopup(message, color) {");
            client.println("var popup = document.createElement('div');");
            client.println("popup.innerHTML = message;");
            client.println("popup.style.backgroundColor = color;");
            client.println("popup.style.color = 'white';");
            client.println("popup.style.padding = '20px';");
            client.println("popup.style.position = 'fixed';");
            client.println("popup.style.top = '50%';");
            client.println("popup.style.left = '50%';");
            client.println("popup.style.transform = 'translate(-50%, -50%)';");
            client.println("popup.style.zIndex = '1000';");
            client.println("popup.style.borderRadius = '10px';");
            client.println("document.body.appendChild(popup);");
            client.println("setTimeout(function () { document.body.removeChild(popup); }, 1000);"); // 1초 후에 팝업 제거
            client.println("}</script>");
            // CSS to style the page
            client.println("<style>");
            client.println("body { background: #f4f7f6; font-family: Arial, sans-serif; margin: 0; padding: 0; }");
            client.println(".sidebar { width: 250px; background: #111; color: #fff; position: fixed; top: 0; left: 0; height: 100%; padding-top: 20px; }");
            client.println(".sidebar a { display: block; color: #bbb; padding: 15px; text-decoration: none; }");
            client.println(".sidebar a:hover { background: #575757; }");
            client.println(".content { margin-left: 250px; padding: 20px; }");
            client.println(".card { background: #fff; margin-bottom: 20px; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }");
            client.println(".card h2 { margin-top: 0; }");
            client.println(".dashboard { display: flex; flex-wrap: wrap; gap: 20px; }");
            client.println(".box { flex: 1 1 calc(50% - 20px); box-sizing: border-box; }");
            client.println(".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }");
            client.println(".header h1 { margin: 0; }");
            client.println(".controls { display: flex; gap: 10px; }");
            client.println(".button { background-color: #00796b; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }");
            client.println(".button:hover { background-color: #004d40; }");
            client.println("canvas { max-width: 100%; height: auto; }");
            client.println("</style></head>");
            
            // Web Page Heading
            client.println("<body><div class='sidebar'><a href='#'>Dashboard</a></div><div class='content'><div class='header'><h1>ESP8266 Weather Station</h1><div class='controls'><button class='button' onclick=\"location.href='/download'\">Data Download</button></div></div><div class='dashboard'>");
            
            // Display temperature and humidity
            client.println("<div class='box'><div class='card'><h2>Temperature</h2><p class='data' id='temperature'>" + String(temperature) + " °C</p></div></div>");
            client.println("<div class='box'><div class='card'><h2>Humidity</h2><p class='data' id='humidity'>" + String(humidity) + " %</p></div></div>");
            
            // Display graphs
            client.println("<div class='box'><div class='card'><h2>Temperature Graph</h2><canvas id='temperatureChart'></canvas></div></div>");
            client.println("<div class='box'><div class='card'><h2>Humidity Graph</h2><canvas id='humidityChart'></canvas></div></div>");
            
            client.println("<script>");
            client.println("var ctxTemp = document.getElementById('temperatureChart').getContext('2d');");
            client.println("var temperatureChart = new Chart(ctxTemp, { type: 'line', data: { labels: [], datasets: [{ label: 'Temperature', data: [], backgroundColor: 'rgba(255, 99, 132, 0.2)', borderColor: 'rgba(255, 99, 132, 1)', borderWidth: 1 }] }, options: { scales: { y: { beginAtZero: true } } } });");
            client.println("var ctxHum = document.getElementById('humidityChart').getContext('2d');");
            client.println("var humidityChart = new Chart(ctxHum, { type: 'line', data: { labels: [], datasets: [{ label: 'Humidity', data: [], backgroundColor: 'rgba(54, 162, 235, 0.2)', borderColor: 'rgba(54, 162, 235, 1)', borderWidth: 1 }] }, options: { scales: { y: { beginAtZero: true } } } });");
            client.println("</script>");
            
            client.println("</div></div></body></html>");
            
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";

    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  if(millis() - t > 2000){
    t = millis();
    esp_now_send(board10, (uint8_t *) &dataToSend, sizeof(dataToSend));
  }
}

//send callback function
void OnDataSent(uint8_t *mac, uint8_t status) {
   if(status == 0){ //Sending completed when status is 0
      Serial.println("성공적으로 송신했음!");
   } else{
      Serial.println("송신 대기중!");
   }
}

//recv callback function
void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  SensorData receivedData;
  memcpy(&receivedData, data, sizeof(receivedData));
  temperature = receivedData.temperature;
  humidity = receivedData.humidity;
  Serial.print("온도: ");
  Serial.print(temperature);
  Serial.print(" °C, 습도: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Update temperature
  temperatureData[dataIndex] = temperature;
  humidityData[dataIndex] = humidity;
  dataIndex = (dataIndex + 1) % dataPoints;

  // Send updated data to all connected WebSocket clients
  String json = "{\"temperature\":" + String(temperature) + ", \"humidity\":" + String(humidity) + "}";
  webSocket.broadcastTXT(json);
}

// Websocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if(type == WStype_TEXT) {
    Serial.printf("[%u] get Text: %s\n", num, payload);
  }
}
