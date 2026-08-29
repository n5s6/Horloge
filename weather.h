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
        for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(5, 5, 10);
        
        // Éclairs aléatoires
        if (random8() < 15) { // Probabilité d'éclair
            int lightningSize = random(10, 40);
            for (int i = 0; i < lightningSize; i++) {
                int randomLed = random(NUM_LEDS/2); // index logique
                setPixelPair(randomLed, CRGB::White);
            }
            if (random8() < 50) { // flash intense global occasionnel
                fill_solid((CRGB*)&leds[0], NUM_LEDS * 4 / 3, CRGB::White); 
                // Attention fill_solid utilise les CRGB raw, NUM_LEDS * 4/3 pour SK6812 hack
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
        
        // Simuler le déplacement vers le bas : on efface tout à chaque frame et on recalcule
        // On utilise l'heure (elapsed) pour faire glisser les gouttes
        for (int x = 0; x < GRID_WIDTH; x++) {
            // Créer une pluie décalée pour chaque colonne
            int yOffset = (elapsed / 40 + x * 7) % (GRID_HEIGHT * 2);
            for (int y = 0; y < GRID_HEIGHT; y++) {
                // Si la goutte passe par cette coordonnée
                if ((y + yOffset) % 15 == 0) {
                     int idx = xy_map(x, y);
                     if (idx != -1) setPixelPair(idx, CRGB(0, 50, 255));
                } else if ((y + yOffset) % 15 == 1) { // Traînée de la goutte
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
            int yOffset = (elapsed / 150 + x * 3) % (GRID_HEIGHT * 3);
            for (int y = 0; y < GRID_HEIGHT; y++) {
                if ((y + yOffset) % 20 == 0) {
                    // Oscillation gauche-droite
                    int wobbleX = x;
                    if (elapsed / 500 % 2 == 0) wobbleX = (x + 1) % GRID_WIDTH;
                    
                    int idx = xy_map(wobbleX, y);
                    if (idx != -1) setPixelPair(idx, CRGB(200, 200, 255)); // Blanc bleuté
                }
            }
        }
    }
    else if (currentWeatherId >= 700 && currentWeatherId < 800) {
        // --- BROUILLARD ---
        // Vagues douces de gris pâle
        float breath = (sin(elapsed * 0.002) + 1.0) / 2.0; // 0.0 à 1.0
        CRGB fogColor = CRGB(
            20 + breath * 30, 
            20 + breath * 30, 
            25 + breath * 35
        );
        for (int i = 0; i < (NUM_LEDS/2); i++) {
            setPixelPair(i, fogColor);
        }
    }
    else if (currentWeatherId == 800) {
        // --- SOLEIL ---
        // Centre jaune, bords qui respirent
        int centerX = GRID_WIDTH / 2;
        int centerY = GRID_HEIGHT / 2;
        float pulse = (sin(elapsed * 0.003) + 1.0) / 2.0; // 0.0 à 1.0
        
        for (int x = 0; x < GRID_WIDTH; x++) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                float dist = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                int idx = xy_map(x, y);
                if (idx != -1) {
                    if (dist < 2.5) {
                        setPixelPair(idx, CRGB(255, 200, 0)); // Coeur brillant
                    } else if (dist < 4.5 + pulse * 1.5) {
                        setPixelPair(idx, CRGB(100 + pulse*50, 50 + pulse*30, 0)); // Halo
                    }
                }
            }
        }
    }
    else if (currentWeatherId > 800) {
        // --- NUAGES ---
        // Massifs gris/blanc qui défilent de gauche à droite
        int xOffset = (elapsed / 200) % GRID_WIDTH;
        for (int x = 0; x < GRID_WIDTH; x++) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                int shiftedX = (x + xOffset) % GRID_WIDTH;
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