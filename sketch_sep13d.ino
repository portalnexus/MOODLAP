#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>
#include <SPI.h>
#include <SD.h>

// ==========================================
// 1. CONFIGURAÇÕES DE REDE (ACCESS POINT)
// ==========================================
const char *ssid = "ESP32-01";
const char *password = "esp32esp";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==========================================
// 2. HARDWARE: OLED, BMP180, SD & LED RGB
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_BMP085 bmp;
bool bmpOk = false;

#define SD_CS_PIN 4
bool sdOk = false;

// Pinos do LED RGB
#define PIN_LED_R 27
#define PIN_LED_G 26
#define PIN_LED_B 25

const bool ANODO_COMUM = false; 

#define PWM_FREQ 5000
#define PWM_RES  8 // 8 bits (0-255)

int redVal = 0;
int greenVal = 0;
int blueVal = 0;

// Telemetria BMP180
float currentTemp = 0.0;
int32_t currentPress = 0;
float currentAlt = 0.0;
unsigned long previousMillis = 0;
const long interval = 2000;

void setRGB(int r, int g, int b) {
  redVal = constrain(r, 0, 255);
  greenVal = constrain(g, 0, 255);
  blueVal = constrain(b, 0, 255);

  int outR = ANODO_COMUM ? (255 - redVal) : redVal;
  int outG = ANODO_COMUM ? (255 - greenVal) : greenVal;
  int outB = ANODO_COMUM ? (255 - blueVal) : blueVal;

  ledcWrite(PIN_LED_R, outR);
  ledcWrite(PIN_LED_G, outG);
  ledcWrite(PIN_LED_B, outB);
}

// ==========================================
// 3. PÁGINAS HTML COM RAW STRING LITERALS
// ==========================================

const char INDEX_HTML[] PROGMEM = R"HTML_CONTENT(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Multi-Station</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Tahoma, sans-serif;
      background: radial-gradient(circle at top, #1e1b4b, #0f172a);
      color: #f8fafc;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      padding: 16px;
    }
    .container { max-width: 600px; width: 100%; }
    .nav-bar { display: flex; gap: 8px; margin-bottom: 16px; }
    .nav-btn {
      flex: 1;
      padding: 12px;
      background: #1e293b;
      color: #94a3b8;
      border: 1px solid #334155;
      border-radius: 10px;
      font-weight: 700;
      cursor: pointer;
      text-align: center;
      font-size: 0.85rem;
    }
    .nav-btn.active { background: #38bdf8; color: #0f172a; border-color: #38bdf8; }
    .panel {
      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 14px;
      padding: 20px;
      margin-bottom: 14px;
    }
    h2 { font-size: 1.2rem; color: #38bdf8; margin-bottom: 14px; }
    
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 10px; }
    .card { background: #0f172a; padding: 14px; border-radius: 10px; border-left: 4px solid #38bdf8; }
    .card-title { font-size: 0.75rem; color: #94a3b8; text-transform: uppercase; font-weight: 600; }
    .card-val { font-size: 1.5rem; font-weight: bold; margin-top: 4px; color: #fff; }
    
    .rgb-preview {
      width: 100%;
      height: 60px;
      border-radius: 10px;
      border: 2px solid #475569;
      margin-bottom: 16px;
      background: rgb(0,0,0);
      transition: background 0.1s;
    }
    .slider-row { margin-bottom: 12px; }
    .slider-row label { display: flex; justify-content: space-between; font-size: 0.85rem; margin-bottom: 4px; font-weight: 600; }
    input[type=range] { width: 100%; height: 8px; border-radius: 4px; outline: none; background: #0f172a; }
    .preset-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin-top: 14px; }
    .preset-btn { padding: 10px 4px; border: none; border-radius: 8px; font-weight: bold; font-size: 0.8rem; cursor: pointer; color: #fff; }

    label { font-size: 0.8rem; color: #94a3b8; display: block; margin-top: 8px; }
    select, input[type=text] {
      width: 100%; padding: 10px; background: #0f172a; border: 1px solid #475569; color: #fff; border-radius: 6px; margin-top: 4px;
    }
    .btn { background: #9333ea; color: white; border: none; padding: 10px; border-radius: 6px; font-weight: bold; cursor: pointer; width: 100%; margin-top: 10px; }
    .btn:hover { background: #7e22ce; }
    .btn-save { background: #10b981; }
    
    #screen-container {
      width: 100%;
      height: 480px;
      background: #000;
      border-radius: 8px;
      margin-top: 12px;
      overflow: hidden;
      border: 1px solid #334155;
    }
    iframe { width: 100%; height: 100%; border: none; }
  </style>
</head>
<body>
  <div class="container">
    <div class="nav-bar">
      <div id="tab1" class="nav-btn active" onclick="switchTab('meteo')">🌤️ ESTAÇÃO</div>
      <div id="tab2" class="nav-btn" onclick="switchTab('rgb')">💡 LED RGB</div>
      <div id="tab3" class="nav-btn" onclick="switchTab('games')">🕹️ GAMES SD</div>
    </div>

    <!-- 1. ABA METEOROLOGIA -->
    <div id="panelMeteo" class="panel">
      <h2>Estação Meteorológica ESP32</h2>
      <div class="grid-2">
        <div class="card">
          <div class="card-title">Temperatura</div>
          <div class="card-val" id="temp">--.- °C</div>
        </div>
        <div class="card" style="border-left-color: #f59e0b;">
          <div class="card-title">Pressão</div>
          <div class="card-val" id="press">---- hPa</div>
        </div>
      </div>
      <div class="card" style="border-left-color: #10b981; margin-bottom: 10px;">
        <div class="card-title">Altitude Estimada</div>
        <div class="card-val" id="alt">---.- m</div>
      </div>
      <div style="display:flex; justify-content:space-between; font-size:0.85rem; color:#94a3b8;">
        <span>Cartão SD:</span>
        <b id="sdStatus" style="color:#34d399;">VERIFICANDO</b>
      </div>
    </div>

    <!-- 2. ABA LED RGB -->
    <div id="panelRGB" class="panel" style="display:none;">
      <h2>Controle do LED RGB</h2>
      <div id="preview" class="rgb-preview"></div>

      <div class="slider-row">
        <label style="color:#f87171;">VERMELHO (R) <span id="valR">0</span></label>
        <input type="range" id="sliderR" min="0" max="255" value="0" oninput="updateRGB()">
      </div>
      <div class="slider-row">
        <label style="color:#4ade80;">VERDE (G) <span id="valG">0</span></label>
        <input type="range" id="sliderG" min="0" max="255" value="0" oninput="updateRGB()">
      </div>
      <div class="slider-row">
        <label style="color:#60a5fa;">AZUL (B) <span id="valB">0</span></label>
        <input type="range" id="sliderB" min="0" max="255" value="0" oninput="updateRGB()">
      </div>

      <div class="preset-grid">
        <button class="preset-btn" style="background:#dc2626;" onclick="setPreset(255,0,0)">Vermelho</button>
        <button class="preset-btn" style="background:#16a34a;" onclick="setPreset(0,255,0)">Verde</button>
        <button class="preset-btn" style="background:#2563eb;" onclick="setPreset(0,0,255)">Azul</button>
        <button class="preset-btn" style="background:#ca8a04;" onclick="setPreset(255,255,0)">Amarelo</button>
        <button class="preset-btn" style="background:#9333ea;" onclick="setPreset(180,0,255)">Roxo</button>
        <button class="preset-btn" style="background:#06b6d4;" onclick="setPreset(0,255,255)">Ciano</button>
        <button class="preset-btn" style="background:#e2e8f0; color:#000;" onclick="setPreset(255,255,255)">Branco</button>
        <button class="preset-btn" style="background:#334155;" onclick="setPreset(0,0,0)">Desligar</button>
      </div>
    </div>

    <!-- 3. ABA JOGOS -->
    <div id="panelGames" class="panel" style="display:none;">
      <h2>Central de Jogos (100% Offline no SD)</h2>
      
      <label>Seu Código de Jogador (Save):</label>
      <input type="text" id="playerId" value="PKM03" maxlength="10">

      <label>Console:</label>
      <select id="consoleSelect" onchange="carregarRomsConsole()">
        <option value="nes">Nintendo 8-bit (NES)</option>
        <option value="gb">Game Boy Clássico (GB)</option>
        <option value="gbc">Game Boy Color (GBC)</option>
        <option value="gba">Game Boy Advance (GBA)</option>
        <option value="snes">Super Nintendo (SNES)</option>
      </select>

      <label>ROMs detectadas no Cartão SD:</label>
      <select id="romList">
        <option value="">Carregando...</option>
      </select>

      <div style="display:flex; gap:8px;">
        <button class="btn" style="flex:2;" onclick="iniciarJogo()">▶️ Carregar e Jogar ROM</button>
        <button class="btn" style="flex:1; background:#0284c7;" onclick="abrirPlayerNovaAba()" title="Jogar em aba separada">⛶ Tela Cheia</button>
      </div>
      
      <div id="screen-container">
        <div style="height:100%; display:flex; align-items:center; justify-content:center; color:#64748b; padding:20px; text-align:center;">
          Selecione o console e a ROM, depois clique em <b>Carregar e Jogar ROM</b>
        </div>
      </div>

      <div style="display:flex; gap: 8px; margin-top: 10px;">
        <button class="btn btn-save" onclick="salvarSaveSD()">💾 Salvar Estado no SD</button>
        <button class="btn" style="background:#475569;" onclick="carregarSaveSD()">📂 Carregar Save do SD</button>
      </div>
      <div id="saveMsg" style="font-size:0.8rem; text-align:center; margin-top:6px; color:#38bdf8;"></div>
    </div>
  </div>

  <script>
    function switchTab(tab) {
      document.getElementById('panelMeteo').style.display = (tab === 'meteo') ? 'block' : 'none';
      document.getElementById('panelRGB').style.display = (tab === 'rgb') ? 'block' : 'none';
      document.getElementById('panelGames').style.display = (tab === 'games') ? 'block' : 'none';
      
      document.getElementById('tab1').className = 'nav-btn ' + ((tab === 'meteo') ? 'active' : '');
      document.getElementById('tab2').className = 'nav-btn ' + ((tab === 'rgb') ? 'active' : '');
      document.getElementById('tab3').className = 'nav-btn ' + ((tab === 'games') ? 'active' : '');
    }

    setInterval(function() {
      fetch('/api/meteo-data')
        .then(function(r) { return r.json(); })
        .then(function(d) {
          document.getElementById('temp').innerText = d.temp.toFixed(1) + ' °C';
          document.getElementById('press').innerText = (d.press / 100).toFixed(1) + ' hPa';
          document.getElementById('alt').innerText = d.alt.toFixed(1) + ' m';
          document.getElementById('sdStatus').innerText = d.sd ? 'GRAVANDO' : 'ERRO';
        });
    }, 2000);

    function updateRGB() {
      var r = document.getElementById('sliderR').value;
      var g = document.getElementById('sliderG').value;
      var b = document.getElementById('sliderB').value;
      document.getElementById('valR').innerText = r;
      document.getElementById('valG').innerText = g;
      document.getElementById('valB').innerText = b;
      document.getElementById('preview').style.background = 'rgb(' + r + ',' + g + ',' + b + ')';
      fetch('/api/set-rgb?r=' + r + '&g=' + g + '&b=' + b);
    }

    function setPreset(r, g, b) {
      document.getElementById('sliderR').value = r;
      document.getElementById('sliderG').value = g;
      document.getElementById('sliderB').value = b;
      updateRGB();
    }

    function carregarRomsConsole() {
      var cons = document.getElementById('consoleSelect').value;
      var sel = document.getElementById('romList');
      sel.innerHTML = '<option value="">Lendo pasta /' + cons + '...</option>';

      fetch('/api/list-roms?console=' + cons)
        .then(function(r) { return r.json(); })
        .then(function(roms) {
          sel.innerHTML = '';
          if(!roms || roms.length === 0) {
            sel.innerHTML = '<option value="">Nenhuma ROM na pasta /' + cons + '</option>';
            return;
          }
          roms.forEach(function(rom) {
            var opt = document.createElement('option');
            opt.value = rom;
            opt.innerText = rom;
            sel.appendChild(opt);
          });
        }).catch(function() {
          sel.innerHTML = '<option value="">Erro ao ler o Cartao SD</option>';
        });
    }
    carregarRomsConsole();

    function iniciarJogo() {
      var cons = document.getElementById('consoleSelect').value;
      var rom = document.getElementById('romList').value;
      if (!rom) { alert('Selecione uma ROM válida!'); return; }

      var container = document.getElementById('screen-container');
      var romUrl = '/roms/' + cons + '/' + encodeURIComponent(rom);
      var playerUrl = '/emu/player.html?console=' + cons + '&rom=' + encodeURIComponent(romUrl);

      container.innerHTML = '<iframe src="' + playerUrl + '" allow="gamepad; autoplay; fullscreen"></iframe>';
    }

    function abrirPlayerNovaAba() {
      var cons = document.getElementById('consoleSelect').value;
      var rom = document.getElementById('romList').value;
      if (!rom) { alert('Selecione uma ROM primeiro!'); return; }
      var romUrl = '/roms/' + cons + '/' + encodeURIComponent(rom);
      window.open('/emu/player.html?console=' + cons + '&rom=' + encodeURIComponent(romUrl), '_blank');
    }

    function salvarSaveSD() {
      var pId = document.getElementById('playerId').value.trim();
      var rom = document.getElementById('romList').value;
      if (!pId || !rom) { alert('Defina o ID do jogador e selecione o jogo!'); return; }

      var payload = JSON.stringify({ playerId: pId, gameId: rom, timestamp: Date.now() });

      fetch('/api/save-game', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ playerId: pId, gameId: rom, payload: payload })
      }).then(function(r) { return r.json(); }).then(function(res) {
        document.getElementById('saveMsg').innerText = 'Save [' + pId + '_' + rom + '.sav] gravado no Cartao SD!';
      });
    }

    function carregarSaveSD() {
      var pId = document.getElementById('playerId').value.trim();
      var rom = document.getElementById('romList').value;
      if (!pId || !rom) return;

      fetch('/api/load-game?playerId=' + pId + '&gameId=' + rom)
        .then(function(r) { return r.json(); })
        .then(function(res) {
          if (res.success) {
            alert('Save localizado no Cartao SD!\nArquivo: /saves/' + rom + '_' + pId + '.sav');
          } else {
            alert('Nenhum save encontrado no SD para este ID e jogo.');
          }
        });
    }
  </script>
</body>
</html>
)HTML_CONTENT";

// ==========================================
// ==========================================
// 4. SUPORTE A ARQUIVOS ESTÁTICOS DO CARTÃO SD
// ==========================================

String urlDecode(const String &text) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len) {
    char c = text[i];
    if (c == '+') {
      decoded += ' ';
      i++;
    } else if (c == '%' && i + 2 < len) {
      temp[2] = text[i + 1];
      temp[3] = text[i + 2];
      decoded += (char)strtol(temp, NULL, 16);
      i += 3;
    } else {
      decoded += c;
      i++;
    }
  }
  return decoded;
}

String getContentType(String filename) {
  if (filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".wasm")) return "application/wasm";
  else if (filename.endsWith(".wav")) return "audio/wav";
  else if (filename.endsWith(".mp3")) return "audio/mpeg";
  else if (filename.endsWith(".zip")) return "application/zip";
  else if (filename.endsWith(".data")) return "application/octet-stream";
  return "application/octet-stream";
}

bool loadFromSD(String path) {
  if (!sdOk) return false;
  path = urlDecode(path);
  int qIdx = path.indexOf('?');
  if (qIdx != -1) path = path.substring(0, qIdx);
  if (path.endsWith("/")) path += "player.html";

  // Fallback inteligente entre versoes wasm e legacy-wasm
  if (!SD.exists(path)) {
    if (path.endsWith("-legacy-wasm.data")) {
      String alt = path;
      alt.replace("-legacy-wasm.data", "-wasm.data");
      if (SD.exists(alt)) path = alt;
    } else if (path.endsWith("-wasm.data") && !path.endsWith("-legacy-wasm.data")) {
      String alt = path;
      alt.replace("-wasm.data", "-legacy-wasm.data");
      if (SD.exists(alt)) path = alt;
    }
  }

  if (SD.exists(path)) {
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    if (file.isDirectory()) {
      file.close();
      return false;
    }
    String contentType = getContentType(path);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    if (server.method() == HTTP_HEAD) {
      server.sendHeader("Content-Length", String(file.size()));
      server.send(200, contentType, "");
      file.close();
      return true;
    }
    Serial.printf("[SD OK] %s (%d bytes)\n", path.c_str(), file.size());
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  Serial.printf("[SD 404] %s\n", path.c_str());
  return false;
}

// ==========================================
// 5. ROTAS DO SERVIDOR WEB
// ==========================================

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleMeteoData() {
  String json = "{";
  json += "\"temp\":" + String(currentTemp, 2) + ",";
  json += "\"press\":" + String(currentPress) + ",";
  json += "\"alt\":" + String(currentAlt, 2) + ",";
  json += "\"sd\":" + String(sdOk ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetRGB() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();
    setRGB(r, g, b);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing params\"}");
  }
}

void handleListRoms() {
  String consoleType = server.arg("console");
  if (consoleType == "") consoleType = "nes";

  if (!sdOk) {
    server.send(500, "application/json", "[]");
    return;
  }

  String dirPath = "/" + consoleType;
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    server.send(200, "application/json", "[]");
    return;
  }

  String json = "[";
  json.reserve(8192);
  bool first = true;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/")) name = name.substring(1);
      if (name.startsWith(consoleType + "/")) name = name.substring(consoleType.length() + 1);

      // Filtra arquivos ocultos e de documentação
      if (!name.startsWith(".") && !name.equalsIgnoreCase("datalog.csv") && !name.endsWith(".txt") && !name.endsWith(".md")) {
        if (!first) json += ",";
        json += "\"" + name + "\"";
        first = false;
      }
    } else {
      // Subpastas (caso existam)
      File subFile = file.openNextFile();
      while (subFile) {
        if (!subFile.isDirectory()) {
          String name = String(subFile.name());
          if (name.startsWith("/")) name = name.substring(1);
          if (name.startsWith(consoleType + "/")) name = name.substring(consoleType.length() + 1);
          if (!name.startsWith(".") && !name.equalsIgnoreCase("datalog.csv") && !name.endsWith(".txt") && !name.endsWith(".md")) {
            if (!first) json += ",";
            json += "\"" + name + "\"";
            first = false;
          }
        }
        subFile = file.openNextFile();
      }
    }
    file = dir.openNextFile();
  }
  json += "]";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleRomStream() {
  String uri = urlDecode(server.uri());
  int qIdx = uri.indexOf('?');
  if (qIdx != -1) uri = uri.substring(0, qIdx);
  String path = uri;
  if (path.startsWith("/roms/")) {
    path = path.substring(5); // remove "/roms" -> "/console/rom"
  } else if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (sdOk && SD.exists(path)) {
    File romFile = SD.open(path, FILE_READ);
    if (romFile && !romFile.isDirectory()) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Cache-Control", "public, max-age=3600");
      if (server.method() == HTTP_HEAD) {
        server.sendHeader("Content-Length", String(romFile.size()));
        server.send(200, "application/octet-stream", "");
        romFile.close();
        return;
      }
      Serial.printf("[ROM OK] %s (%d bytes)\n", path.c_str(), romFile.size());
      server.streamFile(romFile, "application/octet-stream");
      romFile.close();
      return;
    }
  }
  Serial.printf("[ROM 404] %s\n", path.c_str());
  server.send(404, "text/plain", "ROM nao encontrada no SD: " + path);
}

void handleSaveGame() {
  if (!server.hasArg("plain") || !sdOk) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  String body = server.arg("plain");
  int pIdIdx = body.indexOf("\"playerId\":\"");
  int gIdIdx = body.indexOf("\"gameId\":\"");

  if (pIdIdx == -1 || gIdIdx == -1) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  String pId = body.substring(pIdIdx + 12, body.indexOf("\"", pIdIdx + 12));
  String gId = body.substring(gIdIdx + 10, body.indexOf("\"", gIdIdx + 10));

  if (!SD.exists("/saves")) SD.mkdir("/saves");

  String filePath = "/saves/" + gId + "_" + pId + ".sav";
  File saveFile = SD.open(filePath, FILE_WRITE);
  if (saveFile) {
    saveFile.print(body);
    saveFile.close();
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleLoadGame() {
  String pId = server.arg("playerId");
  String gId = server.arg("gameId");
  String filePath = "/saves/" + gId + "_" + pId + ".sav";

  if (SD.exists(filePath)) {
    File saveFile = SD.open(filePath, FILE_READ);
    if (saveFile) {
      String data = saveFile.readString();
      saveFile.close();
      server.send(200, "application/json", "{\"success\":true,\"data\":\"" + data + "\"}");
      return;
    }
  }
  server.send(404, "application/json", "{\"success\":false}");
}

// ==========================================
// 5. SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  // 1. PWM do LED RGB (ESP32 Core 3.x)
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  setRGB(0, 0, 0);

  // 2. Barramento I2C (Display OLED 0x3C e BMP180 0x77)
  Wire.begin(21, 22);

  // Inicializa Display OLED
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  // BMP180
  if (bmp.begin()) {
    bmpOk = true;
    Serial.println(F("[OK] BMP180 detectado."));
  }

  // Cartão SD (Tenta SPI 20MHz para leitura ultra-rápida de ROMs, com fallback)
  if (SD.begin(SD_CS_PIN, SPI, 20000000)) {
    sdOk = true;
    Serial.println(F("[OK] Cartao SD montado (SPI 20MHz)."));
  } else if (SD.begin(SD_CS_PIN)) {
    sdOk = true;
    Serial.println(F("[OK] Cartao SD montado (SPI padrao)."));
  } else {
    Serial.println(F("[ERRO] Falha ao montar o Cartao SD!"));
  }

  if (sdOk) {
    if (!SD.exists("/saves")) SD.mkdir("/saves");
    if (!SD.exists("/datalog.csv")) {
      File f = SD.open("/datalog.csv", FILE_WRITE);
      if (f) {
        f.println("Tempo_ms,Temperatura_C,Pressao_Pa,Altitude_m");
        f.close();
      }
    }
  }

  // Configuração Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);

  // Rotas HTTP
  server.on("/", handleRoot);
  server.on("/api/meteo-data", handleMeteoData);
  server.on("/api/set-rgb", handleSetRGB);
  server.on("/api/list-roms", handleListRoms);
  server.on("/api/save-game", HTTP_POST, handleSaveGame);
  server.on("/api/load-game", handleLoadGame);

  server.onNotFound([]() {
    String uri = server.uri();
    if (uri.startsWith("/roms/")) {
      handleRomStream();
    } else if (loadFromSD(uri)) {
      return;
    } else {
      server.send(404, "text/plain", "Nao encontrado: " + uri);
    }
  });

  server.begin();
  Serial.println(F("[OK] Servidor Web Ativo!"));
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (bmpOk) {
      currentTemp = bmp.readTemperature();
      currentPress = bmp.readPressure();
      currentAlt = bmp.readAltitude();
    }

    if (sdOk && bmpOk) {
      File f = SD.open("/datalog.csv", FILE_APPEND);
      if (f) {
        f.printf("%lu,%.2f,%d,%.2f\n", currentMillis, currentTemp, currentPress, currentAlt);
        f.close();
      }
    }

    // Layout Solicitado no Display OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("SSID: ESP32-01");
    display.println("PSWD: esp32esp");
    display.println("IP:   192.168.4.1");
    display.println("---------------------");

    // Temp | Pressão | Altitude
    if (bmpOk) {
      display.printf("%.1fC|%dhPa|%.0fm\n", currentTemp, (int)(currentPress / 100), currentAlt);
    } else {
      display.println("BMP180: OFFLINE");
    }

    // RGB (R,G,B)
    display.printf("RGB (%d,%d,%d)\n", redVal, greenVal, blueVal);

    display.display();
  }
}