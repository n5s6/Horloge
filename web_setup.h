#ifndef WEB_SETUP_H
#define WEB_SETUP_H

#include "globals.h"

// Forward declaration pour la météo
void updateWeather();

// =====================================================================================
// Déclarations
// =====================================================================================

void resetDisplayModes() {
  fireworksMode = false;
  chaserMode = false;
  pixelTestMode = false;
  weatherMode = false;
  clearLEDs(true);
}

// =====================================================================================
// Section Web - Gestion du serveur et des pages de configuration
// =====================================================================================

void handleFireworks() {
  Serial.println("Feu d'artifice déclenché via le Web !");
  resetDisplayModes();
  fireworksMode = true;
  fireworksStartTime = millis();

  // Réinitialiser les tableaux (variables déclarées dans config.h via
  // globals.h)
  for (int i = 0; i < MAX_ROCKETS; ++i)
    rockets[i].active = false;
  for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i)
    particles[i].active = false;
  activeParticleCount = 0;

  // Rediriger vers la page de statut
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

void handleChaser() {
  Serial.println("Chenillard déclenché via le Web !");
  resetDisplayModes();
  chaserMode = true;
  testPixelIndex = 0;
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

void handlePixelTest() {
  Serial.println("Test case par case déclenché via le Web !");
  resetDisplayModes();
  pixelTestMode = true;
  testPixelIndex = 0;
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

void handleClockMode() {
  Serial.println("Retour au mode Horloge via le Web !");
  resetDisplayModes();
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

void handleWeatherTest() {
  Serial.println("Test Animation Météo via le Web !");
  resetDisplayModes();
  weatherMode = true;
  weatherStartTime = millis();
  updateWeather(); // Force la màj
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

void handleWeatherDebug() {
  if (server.hasArg("weatherId")) {
    int forcedId = server.arg("weatherId").toInt();
    Serial.println("Test Météo forcé avec ID: " + String(forcedId));
    currentWeatherId = forcedId;
    resetDisplayModes();
    weatherMode = true;
    weatherStartTime = millis();
  }
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
}

// =====================================================================================
//  Endpoints API REST (Pour Home Assistant, etc.)
// =====================================================================================

void handleApiStatus() {
  JsonDocument doc;
  String mode = "clock";
  if (fireworksMode) mode = "fireworks";
  else if (weatherMode) mode = "weather";
  else if (chaserMode) mode = "chaser";
  else if (pixelTestMode) mode = "pixeltest";
  
  doc["mode"] = mode;
  doc["brightness"] = FastLED.getBrightness();
  doc["wifi_ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleApiFireworks() {
  Serial.println("API: Déclenchement Feu d'artifice");
  resetDisplayModes();
  fireworksMode = true;
  fireworksStartTime = millis();
  for (int i = 0; i < MAX_ROCKETS; ++i) rockets[i].active = false;
  for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i) particles[i].active = false;
  activeParticleCount = 0;
  server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"fireworks\"}");
}

void handleApiWeather() {
  Serial.println("API: Déclenchement Météo");
  resetDisplayModes();
  weatherMode = true;
  weatherStartTime = millis();
  updateWeather(); 
  server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"weather\"}");
}

void handleApiClock() {
  Serial.println("API: Retour au mode horloge");
  resetDisplayModes();
  server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"clock\"}");
}

void handleApiBrightness() {
  if (server.hasArg("value")) {
    int val = server.arg("value").toInt();
    if (val < 10) val = 10;
    if (val > 250) val = 250;
    brightness = val;
    preferences.putInt("brightness", brightness);
    FastLED.setBrightness(brightness);
    updateLEDs();
    server.send(200, "application/json", "{\"status\":\"ok\",\"brightness\":" + String(brightness) + "}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Paramètre 'value' manquant (10-250)\"}");
  }
}

String crgbToHex(CRGB color) {
  char hex[8];
  sprintf(hex, "#%02x%02x%02x", color.r, color.g, color.b);
  return String(hex);
}

const char *getConfigName(int i) {
  switch (i) {
  case 0:
    return "Minuit / Midi";
  case 1:
    return "1h / 13h";
  case 2:
    return "2h / 14h";
  case 3:
    return "3h / 15h";
  case 4:
    return "4h / 16h (Goûter)";
  case 5:
    return "5h / 17h";
  case 6:
    return "6h / 18h";
  case 7:
    return "7h / 19h";
  case 8:
    return "8h / 20h";
  case 9:
    return "9h / 21h";
  case 10:
    return "10h / 22h";
  case 11:
    return "11h / 23h";
  default:
    return "";
  }
}

void handleStatus() {
  // On utilise un envoi "chunked" car la liste des LEDs peut être très longue
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  String html = R"rawliteral(
  <!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Status Horloge</title>
  <style>
      body{font-family:Arial,sans-serif;background-color:#282c34;color:#ffffff;padding:20px;margin:0;}
      .container{background-color:#3c4049;padding:20px;border-radius:10px;box-shadow:0 4px 8px rgba(0,0,0,0.3);max-width:600px;margin:auto;}
      h1, h2{color:#61dafb;}
      ul{list-style:none;padding:0;}
      li{background:#444;margin:5px 0;padding:10px;border-radius:5px;display:flex;justify-content:space-between;}
      .color-box{width:20px;height:20px;border-radius:3px;display:inline-block;vertical-align:middle;margin-left:10px;}
      a{color:#61dafb;text-decoration:none;font-weight:bold;}
      a:hover{text-decoration:underline;}
  </style><link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Ccircle cx='32' cy='32' r='28' fill='none' stroke='%232c3e50' stroke-width='6'/%3E%3Cpath d='M32 14 v18 h12' fill='none' stroke='%23e74c3c' stroke-width='5' stroke-linecap='round' stroke-linejoin='round'/%3E%3Ccircle cx='32' cy='32' r='3' fill='%232c3e50'/%3E%3C/svg%3E"></head><body><div class="container">
  <h1>État de l'Horloge</h1>
  )rawliteral";
  server.sendContent(html);

  String modeStr = "Horloge";
  if (fireworksMode)
    modeStr = "Feu d'artifice";
  else if (weatherMode)
    modeStr = "Météo";
  else if (chaserMode)
    modeStr = "Chenillard";
  else if (pixelTestMode)
    modeStr = "Test Case par Case";

  String info = "<h2>Informations Générales</h2><ul>";
  info += "<li>Heure actuelle: <span>" + rtc.getTime("%A, %B %d %Y %H:%M:%S") +
          "</span></li>";
  info += "<li>Luminosité globale: <span>" + String(FastLED.getBrightness()) +
          " / 255</span></li>";
  info += "<li>Dernière synchro NTP: <span>il y a " +
          String((millis() - lastNtpSync) / 1000) + " secondes</span></li>";
  info += "<li>Mode actuel: <span>" + modeStr + "</span></li>";
  info += "<li>Wi-Fi SSID: <span>" + WiFi.SSID() + "</span></li>";
  info += "<li>Adresse IP: <span>" + WiFi.localIP().toString() + "</span></li>";
  info += "</ul><hr><h2>LEDs Allumées (Paires logiques)</h2><ul>";
  server.sendContent(info);

  bool anyLedOn = false;
  // Les pixels sont allumés par paire (0-1, 2-3, etc.), on boucle de 2 en 2
  for (int i = 0; i < NUM_LEDS; i += 2) {
    if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
      String hexColor = "#";
      if (leds[i].r < 16)
        hexColor += "0";
      hexColor += String(leds[i].r, HEX);
      if (leds[i].g < 16)
        hexColor += "0";
      hexColor += String(leds[i].g, HEX);
      if (leds[i].b < 16)
        hexColor += "0";
      hexColor += String(leds[i].b, HEX);

      String ledInfo = "<li><span>Paire logic " + String(i / 2) + " (LEDs " +
                       String(i) + " & " + String(i + 1) + ")</span>";
      ledInfo += "<span>RGB(" + String(leds[i].r) + "," + String(leds[i].g) +
                 "," + String(leds[i].b) + ")";
      ledInfo += "<div class='color-box' style='background-color:" + hexColor +
                 ";'></div></span></li>";
      server.sendContent(ledInfo);
      anyLedOn = true;
    }
  }
  if (!anyLedOn) {
    server.sendContent("<li>Aucune LED n'est allumée actuellement.</li>");
  }

  String footer = "</ul><br>";
  footer += "<form action=\"/fireworks\" method=\"POST\" "
            "style=\"margin-bottom: 5px;\"><input type=\"submit\" value=\"🎆 "
            "Lancer Feu d'artifice\" style=\"background-color:#ff5722; "
            "color:#fff; border:none; padding:10px 20px; border-radius:5px; "
            "cursor:pointer; font-weight:bold; width:100%;\"></form>";
  footer += "<form action=\"/weathertest\" method=\"POST\" "
            "style=\"margin-bottom: 5px;\"><input type=\"submit\" value=\"🌤️ "
            "Tester Météo Actuelle\" style=\"background-color:#3498db; "
            "color:#fff; border:none; padding:10px 20px; border-radius:5px; "
            "cursor:pointer; font-weight:bold; width:100%;\"></form>";
  footer += "<form action=\"/weatherdebug\" method=\"POST\" style=\"margin-bottom: 5px; display:flex; gap:5px;\">"
            "<select name=\"weatherId\" style=\"flex:1; padding:10px; border-radius:5px; background:#444; color:#fff; border:1px solid #555; font-size:16px;\">"
            "<option value=\"800\">☀️ Soleil</option><option value=\"802\">☁️ Nuages</option>"
            "<option value=\"500\">🌧️ Pluie</option><option value=\"600\">❄️ Neige</option>"
            "<option value=\"200\">🌩️ Orage</option><option value=\"741\">🌫️ Brouillard</option></select>"
            "<input type=\"submit\" value=\"Débug Météo\" style=\"background-color:#f39c12; color:#fff; border:none; padding:10px; border-radius:5px; cursor:pointer; font-weight:bold;\"></form>";
  footer += "<form action=\"/chaser\" method=\"POST\" style=\"margin-bottom: "
            "5px;\"><input type=\"submit\" value=\"🔄 Lancer Chenillard\" "
            "style=\"background-color:#9c27b0; color:#fff; border:none; "
            "padding:10px 20px; border-radius:5px; cursor:pointer; "
            "font-weight:bold; width:100%;\"></form>";
  footer += "<form action=\"/pixeltest\" method=\"POST\" "
            "style=\"margin-bottom: 5px;\"><input type=\"submit\" value=\"🔍 "
            "Test Case par Case\" style=\"background-color:#00bcd4; "
            "color:#fff; border:none; padding:10px 20px; border-radius:5px; "
            "cursor:pointer; font-weight:bold; width:100%;\"></form>";
  footer += "<form action=\"/clock\" method=\"POST\" style=\"margin-bottom: "
            "15px;\"><input type=\"submit\" value=\"⏱️ Retour Mode Horloge\" "
            "style=\"background-color:#4caf50; color:#fff; border:none; "
            "padding:10px 20px; border-radius:5px; cursor:pointer; "
            "font-weight:bold; width:100%;\"></form>";
  footer += "<a href=\"/\">← Retour à la configuration</a>";
  footer += "</div></body></html>";
  server.sendContent(footer);
  server.sendContent(""); // Terminer l'envoi du corps HTML
}

void handleRoot() {
  String currentSsid = preferences.getString("ssid", "");
  long gmtOffsetHours = preferences.getLong("gmtOffset", 3600) / 3600;
  bool dstEnabled = (preferences.getInt("dstOffset", 3600) != 0);
  int currentBrightness = map(preferences.getInt("brightness", 150), 10, 250, 5,
                              100); // Convertir en pourcentage

  bool bdayInit = preferences.getBool("bdayInit", false);
  if (!bdayInit) {
    preferences.putString("bday0", "01/01");
    preferences.putString("bday1", "24/12");
    preferences.putBool("bdayInit", true);
  }

  String htmlHead =
      R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Horloge</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #282c34; color: #ffffff; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px 0; }
        .container { background-color: #3c4049; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); text-align: center; width: 90%; max-width: 400px; }
        h1, h2 { color: #61dafb; }
        form { display: flex; flex-direction: column; gap: 15px; }
        .input-group { display: flex; flex-direction: column; align-items: flex-start; }
        .input-group-row { flex-direction: row; align-items: center; gap: 10px; width: 100%; justify-content: space-between; }
        label { margin-bottom: 5px; }
        input[type='text'], input[type='password'], input[type='number'] { width: 95%; padding: 8px; border-radius: 5px; border: 1px solid #555; background-color: #444; color: #fff; }
        input[type='range'] { width: 80%; }
        input[type='color'] { border: none; width: 45%; height: 35px; border-radius: 5px; background: none; cursor: pointer; }
        input[type='submit'] { background-color: #61dafb; color: #282c34; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; transition: background-color: 0.3s; margin-top: 10px; }
        input[type='submit']:hover { background-color: #21a1f2; }
        hr { border: 1px solid #555; margin: 20px 0; }
    </style>
    <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Ccircle cx='32' cy='32' r='28' fill='none' stroke='%232c3e50' stroke-width='6'/%3E%3Cpath d='M32 14 v18 h12' fill='none' stroke='%23e74c3c' stroke-width='5' stroke-linecap='round' stroke-linejoin='round'/%3E%3Ccircle cx='32' cy='32' r='3' fill='%232c3e50'/%3E%3C/svg%3E">
</head>
<body>
    <div class="container">
        <h1>Configuration</h1>
        <form action="/saveconfig" method="POST">
            <h2>Réseau Wi-Fi</h2>
            <div class="input-group">
                <label for="ssid">SSID (Nom du Wi-Fi):</label>
                <input type="text" id="ssid" name="ssid" value=")rawliteral" +
      currentSsid + R"rawliteral(" required>
            </div>
            <div class="input-group">
                <label for="pass">Mot de passe:</label>
                <input type="password" id="pass" name="pass">
            </div>
            <hr>
            <h2>Fuseau Horaire</h2>
            <div class="input-group">
                <label for="gmtOffset">Décalage UTC (heures):</label>
                <input type="number" id="gmtOffset" name="gmtOffset" min="-12" max="14" step="1" value=")rawliteral" +
      String(gmtOffsetHours) + R"rawliteral(" required>
            </div>
            <div class="input-group" style="flex-direction: row; align-items: center; gap: 10px;">
                <input type="checkbox" id="dst" name="dst" )rawliteral" +
      (dstEnabled ? "checked" : "") + R"rawliteral(>
                <label for="dst" style="margin-bottom:0;">Activer l'heure d'été</label>
            </div>
            <hr>
            <h2>Luminosité</h2>
            <div class="input-group">
                <label for="brightness">Luminosité: <span id="brightnessValue">)rawliteral" +
      String(currentBrightness) + R"rawliteral(%</span></label>
                <input type="range" id="brightness" name="brightness" min="5" max="100" value=")rawliteral" +
      String(currentBrightness) +
      R"rawliteral(" oninput="document.getElementById('brightnessValue').innerText = this.value + '%'">
            </div>
            <hr>
            <h2>Météo (OpenWeatherMap)</h2>
            <div class="input-group" style="flex-direction: row; align-items: center; gap: 10px;">
                <input type="checkbox" id="weatherEnabled" name="weatherEnabled" )rawliteral" +
      (weatherEnabled ? "checked" : "") + R"rawliteral(>
                <label for="weatherEnabled" style="margin-bottom:0;">Activer l'animation météo horaire</label>
            </div>
            <div class="input-group">
                <label for="weatherCity">Ville (ex: Paris,FR)</label>
                <input type="text" id="weatherCity" name="weatherCity" value=")rawliteral" +
      weatherCity + R"rawliteral(">
            </div>
            <div class="input-group">
                <label for="weatherApiKey">Clé API OpenWeatherMap</label>
                <input type="text" id="weatherApiKey" name="weatherApiKey" value=")rawliteral" +
      weatherApiKey + R"rawliteral(">
            </div>
            <hr>
            <h2>Couleurs par Heure</h2>
)rawliteral";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(htmlHead);

  String bdayHtml = "<h2>Anniversaires (Feux d'artifice horaires)</h2><p "
                    "style='font-size: 0.9em; margin-top: -10px;'>Format: "
                    "JJ/MM (laissez vide pour désactiver)</p>";
  for (int i = 0; i < 10; i++) {
    String key = "bday" + String(i);
    String val = preferences.getString(key.c_str(), "");
    bdayHtml += "<div class='input-group-row' style='margin-bottom: 5px;'>";
    bdayHtml += "<label for='" + key + "' style='width: 60px;'>Date " +
                String(i + 1) + ":</label>";
    bdayHtml +=
        "<input type='text' id='" + key + "' name='" + key + "' value='" + val +
        "' placeholder='JJ/MM' pattern='([0-2][0-9]|3[0-1])/(0[1-9]|1[0-2])' "
        "style='width: 100px;'>";
    bdayHtml += "</div>";
  }
  bdayHtml += "<hr>";
  server.sendContent(bdayHtml);

  for (int i = 0; i < 12; i++) {
    String colorChunk = "<div class='input-group'><label><strong>" +
                        String(getConfigName(i)) + "</strong></label>";
    colorChunk += "<div class='input-group-row'>";
    colorChunk += "<input type='color' name='colorBase" + String(i) +
                  "' value='" + crgbToHex(displayColors[i][0]) +
                  "' title='Couleur de la phrase (Base)'>";
    colorChunk += "<input type='color' name='colorProg" + String(i) +
                  "' value='" + crgbToHex(displayColors[i][1]) +
                  "' title='Couleur du mot (Progression)'>";
    colorChunk += "</div></div>";
    server.sendContent(colorChunk);
  }

  String htmlFoot = R"rawliteral(
            <input type="submit" value="Enregistrer & Redémarrer">
        </form>
        <hr>
        <h2>Tests & Animations</h2>
        <form action="/fireworks" method="POST" style="margin-bottom: 5px;"><input type="submit" value="🎆 Lancer Feu d'Artifice" style="background-color: #ff5722; width: 100%; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;"></form>
        <form action="/weathertest" method="POST" style="margin-bottom: 5px;"><input type="submit" value="🌤️ Tester Météo Actuelle" style="background-color: #3498db; width: 100%; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;"></form>
        
        <form action="/weatherdebug" method="POST" style="margin-bottom: 5px; display:flex; gap:5px;">
            <select name="weatherId" style="flex:1; padding:10px; border-radius:5px; background:#444; color:#fff; border:1px solid #555; font-size:16px;">
                <option value="800">☀️ Soleil</option>
                <option value="802">☁️ Nuages</option>
                <option value="500">🌧️ Pluie</option>
                <option value="600">❄️ Neige</option>
                <option value="200">🌩️ Orage</option>
                <option value="741">🌫️ Brouillard</option>
            </select>
            <input type="submit" value="Débug Météo" style="background-color: #f39c12; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;">
        </form>

        <form action="/chaser" method="POST" style="margin-bottom: 5px;"><input type="submit" value="🔄 Lancer Chenillard" style="background-color: #9c27b0; width: 100%; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;"></form>
        <form action="/pixeltest" method="POST" style="margin-bottom: 5px;"><input type="submit" value="🔍 Test Case par Case" style="background-color: #00bcd4; width: 100%; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;"></form>
        <form action="/clock" method="POST"><input type="submit" value="⏱️ Retour Mode Horloge" style="background-color: #4caf50; width: 100%; border:none; padding:10px; border-radius:5px; color:#fff; cursor:pointer; font-weight:bold;"></form>
        <hr>
        <a href="/status" style="display:block; margin-top:15px; text-decoration:none;">Voir l'état détaillé de l'horloge</a>
    </div>
</body>
</html>
)rawliteral";
  server.sendContent(htmlFoot);
  server.sendContent(""); // Fin de la réponse HTTP chunked
}

void handleSaveConfig() {
  // Sauvegarde de la configuration Wi-Fi
  if (server.hasArg("ssid")) {
    preferences.putString("ssid", server.arg("ssid"));
    Serial.println("SSID saved: " + server.arg("ssid"));
  }

  if (server.hasArg("pass") && server.arg("pass").length() > 0) {
    preferences.putString("password", server.arg("pass"));
    Serial.println("Password saved.");
  }

  // Sauvegarde du fuseau horaire
  if (server.hasArg("gmtOffset")) {
    long gmtOffset_hours = server.arg("gmtOffset").toInt();
    preferences.putLong("gmtOffset", gmtOffset_hours * 3600);
    Serial.println("GMT Offset saved (seconds): " +
                   String(gmtOffset_hours * 3600));
  }

  int dstOffset_sec = server.hasArg("dst") ? 3600 : 0;
  preferences.putInt("dstOffset", dstOffset_sec);
  Serial.println("Daylight Saving Time Offset saved (seconds): " +
                 String(dstOffset_sec));

  // Sauvegarde de la luminosité
  if (server.hasArg("brightness")) {
    int brightnessPercent = server.arg("brightness").toInt();
    int brightnessValue = map(brightnessPercent, 5, 100, 10, 250);
    preferences.putInt("brightness", brightnessValue);
    Serial.println("Brightness saved: " + String(brightnessValue));
  }

  // Sauvegarde de la météo
  bool wEnabled = server.hasArg("weatherEnabled");
  preferences.putBool("weatherEnabled", wEnabled);
  if (server.hasArg("weatherCity")) {
    preferences.putString("weatherCity", server.arg("weatherCity"));
  }
  if (server.hasArg("weatherApiKey")) {
    preferences.putString("weatherApiKey", server.arg("weatherApiKey"));
  }
  Serial.println("Weather config saved. Enabled: " + String(wEnabled));

  // Sauvegarde des anniversaires
  for (int i = 0; i < 10; i++) {
    String key = "bday" + String(i);
    if (server.hasArg(key)) {
      preferences.putString(key.c_str(), server.arg(key));
    }
  }

  // Sauvegarde des couleurs personnalisées
  for (int i = 0; i < 12; i++) {
    String keyBase = "colorBase" + String(i);
    String keyProg = "colorProg" + String(i);

    if (server.hasArg(keyBase)) {
      String hex = server.arg(keyBase);
      if (hex.startsWith("#") && hex.length() == 7) {
        uint32_t c = strtoul(hex.substring(1).c_str(), NULL, 16);
        preferences.putUInt(keyBase.c_str(), c);
      }
    }
    if (server.hasArg(keyProg)) {
      String hex = server.arg(keyProg);
      if (hex.startsWith("#") && hex.length() == 7) {
        uint32_t c = strtoul(hex.substring(1).c_str(), NULL, 16);
        preferences.putUInt(keyProg.c_str(), c);
      }
    }
  }

  String successMessage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8"><title>Configuration Enregistrée</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #282c34; color: #ffffff; text-align: center; padding-top: 50px; }
        .container { background-color: #3c4049; padding: 20px; border-radius: 10px; display: inline-block; }
        h1 { color: #98c379; }
        p { font-size: 1.1em; }
    </style>
    <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Ccircle cx='32' cy='32' r='28' fill='none' stroke='%232c3e50' stroke-width='6'/%3E%3Cpath d='M32 14 v18 h12' fill='none' stroke='%23e74c3c' stroke-width='5' stroke-linecap='round' stroke-linejoin='round'/%3E%3Ccircle cx='32' cy='32' r='3' fill='%232c3e50'/%3E%3C/svg%3E">
</head>
<body>
    <div class="container">
        <h1>Configuration enregistrée !</h1>
        <p>L'horloge va redémarrer pour appliquer les nouveaux paramètres...</p>
    </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", successMessage);

  delay(2000);
  ESP.restart();
}

void displayConfigModePattern() {
  clearLEDs(false);
  for (int i = 0; i < NUM_LEDS / 2; i += 2) {
    setPixelPair(i, CRGB::Blue);
  }
  updateLEDs();
}

void startAPMode() {
  apModeActive = true;
  Serial.println("Configuring access point...");
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/saveconfig", HTTP_POST, handleSaveConfig);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/fireworks", HTTP_POST, handleFireworks);
  server.on("/weathertest", HTTP_POST, handleWeatherTest);
  server.on("/weatherdebug", HTTP_POST, handleWeatherDebug);
  server.on("/chaser", HTTP_POST, handleChaser);
  server.on("/pixeltest", HTTP_POST, handlePixelTest);
  server.on("/clock", HTTP_POST, handleClockMode);

  // Routes API
  server.on("/api/status", HTTP_ANY, handleApiStatus);
  server.on("/api/fireworks", HTTP_ANY, handleApiFireworks);
  server.on("/api/weather", HTTP_ANY, handleApiWeather);
  server.on("/api/clock", HTTP_ANY, handleApiClock);
  server.on("/api/brightness", HTTP_ANY, handleApiBrightness);

  server.begin();
  Serial.println(
      "HTTP server started. Open http://192.168.4.1 in your browser.");

  // Afficher un pattern visuel pour indiquer le mode configuration
  displayConfigModePattern();
}

void syncNtpTime() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot sync NTP time, WiFi not connected.");
    return;
  }

  Serial.println("Synchronizing time with NTP server...");
  long gmtOffset_sec =
      preferences.getLong("gmtOffset", 3600); // Par défaut sur Paris UTC+1
  int daylightOffset_sec =
      preferences.getInt("dstOffset", 3600); // Heure d'été activée par défaut

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
    lastNtpSync = millis(); // Mettre à jour le temps de la dernière synchro
    Serial.println("Time synchronized: " +
                   rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  } else {
    Serial.println("Failed to obtain time from NTP server.");
  }
}

void connectToWiFi() {
  String storedSsid = preferences.getString("ssid", "");
  if (storedSsid.length() > 0) {
    Serial.print("Connexion à " + storedSsid);
    String storedPass = preferences.getString("password", "");
    WiFi.begin(storedSsid.c_str(), storedPass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnecté ! IP: " + WiFi.localIP().toString());
      syncNtpTime();

      // Démarrer le serveur web sur le réseau local
      server.on("/", HTTP_GET, handleRoot);
      server.on("/saveconfig", HTTP_POST, handleSaveConfig);
      server.on("/status", HTTP_GET, handleStatus);
      server.on("/fireworks", HTTP_POST, handleFireworks);
      server.on("/weathertest", HTTP_POST, handleWeatherTest);
      server.on("/weatherdebug", HTTP_POST, handleWeatherDebug);
      server.on("/chaser", HTTP_POST, handleChaser);
      server.on("/pixeltest", HTTP_POST, handlePixelTest);
      server.on("/clock", HTTP_POST, handleClockMode);

      // Routes API
      server.on("/api/status", HTTP_ANY, handleApiStatus);
      server.on("/api/fireworks", HTTP_ANY, handleApiFireworks);
      server.on("/api/weather", HTTP_ANY, handleApiWeather);
      server.on("/api/clock", HTTP_ANY, handleApiClock);
      server.on("/api/brightness", HTTP_ANY, handleApiBrightness);

      server.begin();
      Serial.println("Serveur web démarré sur le réseau local.");
    } else {
      Serial.println(
          "\nÉchec de la connexion. Démarrage du mode Point d'Accès.");
      startAPMode();
    }
  } else {
    Serial.println(
        "Aucune configuration Wi-Fi. Démarrage du mode Point d'Accès.");
    startAPMode();
  }
}

#endif // WEB_SETUP_H
