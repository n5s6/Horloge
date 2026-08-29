#ifndef WEATHER_H
#define WEATHER_H

#include "globals.h"

// =====================================================================================
//  WEATHER.H — Animations météorologiques
//
//  Récupère la météo actuelle via OpenWeatherMap et l'affiche brièvement 
//  au début de chaque heure si activé.
// =====================================================================================

/**
 * @brief Met à jour les données météo depuis OpenWeatherMap.
 * Doit être appelé toutes les 30 à 60 minutes.
 */
void updateWeather() {
    if (!weatherEnabled || weatherApiKey == "" || weatherCity == "") {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Mise à jour de la météo...");
        HTTPClient http;
        String url = "http://api.openweathermap.org/data/2.5/weather?q=" + weatherCity + "&appid=" + weatherApiKey;
        http.begin(url);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            // Allouer un document JSON (taille estimée 1024 octets suffisante pour la réponse météo basique)
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                currentWeatherId = doc["weather"][0]["id"].as<int>();
                Serial.print("Météo ID reçu : ");
                Serial.println(currentWeatherId);
            } else {
                Serial.print("Erreur de parsing JSON : ");
                Serial.println(error.c_str());
            }
        } else {
            Serial.print("Erreur HTTP Météo : ");
            Serial.println(httpCode);
        }
        http.end();
        lastWeatherSync = millis();
    }
}

/**
 * @brief Affiche l'animation correspondant à la météo actuelle.
 * 
 * Les codes OpenWeatherMap :
 * 2xx : Orage (Thunderstorm)
 * 3xx : Bruine (Drizzle)
 * 5xx : Pluie (Rain)
 * 6xx : Neige (Snow)
 * 7xx : Brouillard / Atmosphère (Atmosphere)
 * 800 : Clair / Soleil (Clear)
 * 80x : Nuages (Clouds)
 */
void renderWeatherAnimation() {
    unsigned long elapsed = millis() - weatherStartTime;
    clearLEDs(false);

    if (currentWeatherId >= 200 && currentWeatherId < 300) {
        // --- ORAGE (Éclairs) ---
        // Fond nuageux (gris bleuté très sombre)
        for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(2, 2, 8);
        
        // Éclairs frénétiques
        int lightningChance = (elapsed % 3000 < 500) ? 80 : 5; // Salves d'éclairs toutes les 3 secondes
        if (random8() < lightningChance) {
            int lightningSize = random(10, 60);
            for (int i = 0; i < lightningSize; i++) {
                int randomLed = random(NUM_LEDS/2); 
                setPixelPair(randomLed, CRGB(200, 220, 255)); // Blanc bleuté intense
            }
            if (random8() < 80) { // Gros flash très fréquent pendant la salve
                fill_solid((CRGB*)&leds[0], NUM_LEDS * 4 / 3, CRGB(180, 200, 255)); 
            }
        }
    } 
    else if ((currentWeatherId >= 300 && currentWeatherId < 600) || currentWeatherId >= 500 && currentWeatherId < 600) {
        // --- PLUIE / BRUINE ---
        // Des "gouttes" bleues qui tombent
        // On utilise la persistance avec fadeToBlackBy (mais on ne peut pas utiliser fadeToBlackBy facilement sur le hack RGBW)
        // Donc on recalcule une traînée ou on allume aléatoirement en décalant vers le bas
        
        // Au lieu de fadeToBlackBy (complexe avec le hack RGBW), 
        // on va juste générer de nouvelles gouttes aléatoires
        
        // Simuler le déplacement vers le bas
        for (int x = 0; x < GRID_WIDTH; x++) {
            // Créer une pluie décalée pour chaque colonne
            int yOffset = (elapsed / 30 + x * 7) % (GRID_HEIGHT * 2);
            for (int y = 0; y < GRID_HEIGHT; y++) {
                // y - yOffset pour faire tomber du haut vers le bas
                int yPos = (y - yOffset + GRID_HEIGHT * 2) % 15;
                if (yPos == 0) {
                     int idx = xy_map(x, y);
                     if (idx != -1) setPixelPair(idx, CRGB(0, 80, 255));
                } else if (yPos == 1 || yPos == 2) { // Traînée de la goutte plus longue
                     int idx = xy_map(x, y);
                     if (idx != -1) setPixelPair(idx, CRGB(0, 20, 100));
                }
            }
        }
    }
    else if (currentWeatherId >= 600 && currentWeatherId < 700) {
        // --- NEIGE ---
        // Particules blanches qui tombent très lentement et oscillent
        for (int x = 0; x < GRID_WIDTH; x++) {
            int yOffset = (elapsed / 150 + x * 5) % (GRID_HEIGHT * 3);
            for (int y = 0; y < GRID_HEIGHT; y++) {
                int yPos = (y - yOffset + GRID_HEIGHT * 3) % 20;
                if (yPos == 0) {
                    // Oscillation gauche-droite
                    int wobbleX = x;
                    if (elapsed / 300 % 2 == 0) wobbleX = (x + 1) % GRID_WIDTH;
                    
                    int idx = xy_map(wobbleX, y);
                    if (idx != -1) setPixelPair(idx, CRGB(200, 200, 255)); // Blanc bleuté
                }
            }
        }
    }
    else if (currentWeatherId >= 700 && currentWeatherId < 800) {
        // --- BROUILLARD ---
        // Volutes de brouillard plus denses, rapides et contrastées
        for (int x = 0; x < GRID_WIDTH; x++) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                // Double onde croisée plus rapide pour simuler des volutes de fumée
                float wave1 = sin((x + elapsed * 0.004) * 0.6 + y * 0.3);
                float wave2 = cos((y - elapsed * 0.003) * 0.7 + x * 0.4);
                float intensity = (wave1 + wave2 + 2.0) / 4.0; // Normalisé entre 0.0 et 1.0
                
                // Contraste augmenté : de gris très foncé à gris/blanc bleuté bien visible
                CRGB fogColor = CRGB(
                    10 + intensity * 120, 
                    10 + intensity * 120, 
                    15 + intensity * 140
                );
                int idx = xy_map(x, y);
                if (idx != -1) setPixelPair(idx, fogColor);
            }
        }
    }
    else if (currentWeatherId == 800) {
        // --- SOLEIL ---
        // Cœur petit, grands rayons tournants ou pulsatiles
        int centerX = GRID_WIDTH / 2;
        int centerY = GRID_HEIGHT / 2;
        float pulse = (sin(elapsed * 0.004) + 1.0) / 2.0;
        
        for (int x = 0; x < GRID_WIDTH; x++) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                float dist = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                int idx = xy_map(x, y);
                if (idx != -1) {
                    if (dist < 1.5) {
                        // Petit cœur très brillant
                        setPixelPair(idx, CRGB(255, 220, 0)); 
                    } 
                    else if (dist < 2.5) {
                        // Halo proche
                        setPixelPair(idx, CRGB(150, 80, 0));
                    }
                    else if ((x == centerX || y == centerY || x == y || x == (GRID_HEIGHT - 1 - y)) && dist < 5.5 + pulse * 1.5) {
                        // Rayons qui s'étirent (croix et diagonales)
                        int brightness = 80 - dist * 10 + pulse * 40;
                        if (brightness < 0) brightness = 0;
                        setPixelPair(idx, CRGB(brightness, brightness * 0.6, 0));
                    }
                }
            }
        }
    }
    else if (currentWeatherId > 800) {
        // --- NUAGES ---
        // Massifs gris/blanc qui défilent de GAUCHE à DROITE
        int xOffset = (elapsed / 250) % GRID_WIDTH;
        for (int x = 0; x < GRID_WIDTH; x++) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                // x - xOffset pour un défilement de gauche à droite
                int shiftedX = (x - xOffset + GRID_WIDTH * 2) % GRID_WIDTH;
                int idx = xy_map(x, y);
                if (idx != -1) {
                    // Forme de nuage basique via une formule simple
                    if ((shiftedX > 2 && shiftedX < 8 && y > 3 && y < 7) || 
                        (shiftedX > 4 && shiftedX < 10 && y > 2 && y < 6)) {
                        setPixelPair(idx, CRGB(150, 150, 160)); // Nuage blanc/gris
                    } else if (currentWeatherId > 802 && // Très nuageux
                              ((shiftedX < 4 && y > 5 && y < 9) || 
                               (shiftedX > 7 && y > 4 && y < 8))) {
                        setPixelPair(idx, CRGB(80, 80, 90)); // Nuages foncés
                    }
                }
            }
        }
    }

    updateLEDs();
}

#endif // WEATHER_H