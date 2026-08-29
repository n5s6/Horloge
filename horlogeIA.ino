// =====================================================================================
//  HORLOGEIA.INO — Programme principal de l'horloge à mots française
//
//  Ce fichier contient :
//    - La définition des variables globales
//    - setup() : initialisation matérielle, chargement config, connexion Wi-Fi
//    - loop()  : boucle principale (bouton, modes d'affichage, NTP)
//    - Fonctions d'affichage : heure, luminosité, tests LED
//
//  Matériel : ESP32-S3 + ruban SK6812 RGBW (288 LEDs = 144 paires logiques)
// =====================================================================================

#define FASTLED_INTERNAL // Supprime les messages de compilation internes de
                         // FastLED

#include "fireworks.h"
#include "weather.h"
#include "globals.h"
#include "web_setup.h"

// =====================================================================================
//  VARIABLES GLOBALES
// =====================================================================================

// --- Buffer LED principal ---
CRGBW leds[NUM_LEDS];

// --- Horloge et persistance ---
ESP32Time rtc(0);        // Offset 0 : le décalage UTC est géré via configTime()
Preferences preferences; // Stockage persistant en flash NVS
WebServer server(80);    // Serveur web sur le port 80

// --- Drapeaux d'état ---
bool apModeActive = false;  // Mode Point d'Accès actif
bool fireworksMode = false; // Animation feu d'artifice en cours
bool chaserMode = false;    // Animation chenillard en cours
bool pixelTestMode = false; // Test LED case par case en cours
bool weatherMode = false;   // Animation météo en cours
int testPixelIndex = 0;     // Index courant pour les animations de test

// --- Drapeaux d'état (Météo)
bool weatherEnabled = false;
String weatherApiKey = "";
String weatherCity = "";
int currentWeatherId = 800; // Clair par défaut
unsigned long lastWeatherSync = 0;
unsigned long weatherStartTime = 0;
bool weatherTriggeredThisHour = false;

// --- Drapeaux d'état (Anniversaires)
bool isBirthdayToday = false;
int birthdayFireworkMinute = -1;
bool fireworkTriggeredThisHour = false;
int lastHourChecked = -1;
int lastDayChecked = -1;

// --- Timestamps et compteurs ---
unsigned long fireworksStartTime = 0;
unsigned long lastButtonPressTime = 0;
int activeParticleCount = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastNtpSync = 0;
int brightness = 150; // Luminosité par défaut (plage 10–250)

// --- Données d'animation ---
Rocket rockets[MAX_ROCKETS];
Particle particles[MAX_TOTAL_PARTICLES];
CRGB displayColors[12]
                  [2]; // Couleurs configurées : [heure][0=base, 1=progression]

// =====================================================================================
//  SETUP — Initialisation au démarrage
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nBOOTING ESP32 WORD CLOCK");

  // Ouvrir le namespace de stockage persistant
  preferences.begin("horloge-ia", false);

  // Configurer le bouton physique avec résistance pull-up interne
  pinMode(btnSetting, INPUT_PULLUP);
  delay(1000);

  // --- Charger la luminosité sauvegardée (ou valeur par défaut) ---
  brightness = preferences.getInt("brightness", 150);

  // --- Charger les paramètres météo ---
  weatherEnabled = preferences.getBool("weatherEnabled", false);
  weatherApiKey = preferences.getString("weatherApiKey", "");
  weatherCity = preferences.getString("weatherCity", "");

  // --- Charger les couleurs personnalisées (ou couleurs par défaut PROGMEM)
  // ---
  for (int i = 0; i < 12; i++) {
    String keyBase = "colorBase" + String(i);
    String keyProg = "colorProg" + String(i);

    // Lire les couleurs par défaut depuis la mémoire programme (flash)
    CRGB defBase, defProg;
    memcpy_P((void *)&defBase, (const void *)&defaultColors_PGM[i][0],
             sizeof(CRGB));
    memcpy_P((void *)&defProg, (const void *)&defaultColors_PGM[i][1],
             sizeof(CRGB));

    // Encoder en uint32 pour le stockage/lecture dans Preferences
    uint32_t defBaseInt = (defBase.r << 16) | (defBase.g << 8) | defBase.b;
    uint32_t defProgInt = (defProg.r << 16) | (defProg.g << 8) | defProg.b;

    // Charger la valeur sauvegardée (ou le défaut si aucune sauvegarde)
    uint32_t cBase = preferences.getUInt(keyBase.c_str(), defBaseInt);
    uint32_t cProg = preferences.getUInt(keyProg.c_str(), defProgInt);

    // Décoder et stocker en RAM
    displayColors[i][0] =
        CRGB((cBase >> 16) & 0xFF, (cBase >> 8) & 0xFF, cBase & 0xFF);
    displayColors[i][1] =
        CRGB((cProg >> 16) & 0xFF, (cProg >> 8) & 0xFF, cProg & 0xFF);
  }

  // --- Initialiser le ruban LED SK6812 RGBW ---
  // Le hack "Type Punning" est utilisé car le support natif RGBW (setRgbw)
  // pose souvent problème sur les drivers ESP32.
  // On alloue virtuellement (NUM_LEDS * 4 / 3) pixels CRGB et on indique 
  // l'ordre RGB (FastLED lit alors séquentiellement la mémoire structurée en GRBW).
  int num_crgb_pixels = (NUM_LEDS * 4) / 3;
  FastLED.addLeds<WS2812, LED_PIN, RGB>((CRGB*)&leds[0], num_crgb_pixels);
  FastLED.setBrightness(brightness);
  clearLEDs(true);

  // Initialisation du générateur aléatoire (très entropique sur ESP32)
  randomSeed(esp_random());

  // --- Connexion Wi-Fi (ou démarrage en mode Point d'Accès) ---
  connectToWiFi();

  // Si connecté au Wi-Fi, démarrer l'horloge avec un feu d'artifice de
  // bienvenue
  if (!apModeActive) {
    Serial.println("BOOTED. Current time: " +
                   rtc.getTime("%A, %B %d %Y %H:%M:%S"));

    // Lancer le feu d'artifice de démarrage
    Serial.println("Activating Fireworks at startup!");
    fireworksMode = true;
    fireworksStartTime = millis();
    for (int i = 0; i < MAX_ROCKETS; ++i)
      rockets[i].active = false;
    for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i)
      particles[i].active = false;
    activeParticleCount = 0;
    clearLEDs(true);
  }
}

// =====================================================================================
//  LOOP — Boucle principale
// =====================================================================================
void loop() {
  // Toujours écouter les requêtes web (mode AP ou réseau local)
  server.handleClient();

  // En mode Point d'Accès, ne pas exécuter la logique horloge
  if (apModeActive) {
    return;
  }

  // --- Resynchronisation NTP périodique (toutes les 24h) ---
  if (millis() - lastNtpSync > ntpSyncInterval) {
    syncNtpTime();
  }

  // --- Gestion des anniversaires (Feux d'artifice horaires) ---
  int currentDay = rtc.getDay();
  int currentHour = rtc.getHour(true);

  if (currentDay != lastDayChecked) {
    lastDayChecked = currentDay;
    String todayStr = rtc.getTime("%d/%m"); // ex: "01/01"
    isBirthdayToday = false;
    for (int i = 0; i < 10; i++) {
      String key = "bday" + String(i);
      String savedDate = preferences.getString(key.c_str(), "");
      if (savedDate == todayStr) {
        isBirthdayToday = true;
        Serial.println("🎉 Aujourd'hui est un jour d'anniversaire ! (" +
                       todayStr + ")");
        break;
      }
    }
  }

  if (currentHour != lastHourChecked) {
    lastHourChecked = currentHour;
    if (isBirthdayToday) {
      birthdayFireworkMinute = random(0, 60);
      fireworkTriggeredThisHour = false;
      Serial.println("🎇 Prochain feu d'artifice prévu à la minute : " +
                     String(birthdayFireworkMinute));
    }
  }

  if (isBirthdayToday && !fireworksMode && !chaserMode && !pixelTestMode && !weatherMode) {
    if (rtc.getMinute() == birthdayFireworkMinute && rtc.getSecond() == 0 &&
        !fireworkTriggeredThisHour) {
      Serial.println(
          "🎆 C'est l'heure de l'anniversaire ! Lancement du feu d'artifice !");
      fireworkTriggeredThisHour = true;

      fireworksMode = true;
      fireworksStartTime = millis();
      for (int i = 0; i < MAX_ROCKETS; ++i)
        rockets[i].active = false;
      for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i)
        particles[i].active = false;
      activeParticleCount = 0;
      clearLEDs(true);
    }
  }

  // --- Mise à jour Météo Périodique (toutes les 30 minutes) ---
  if (weatherEnabled && (millis() - lastWeatherSync > 1800000 || lastWeatherSync == 0)) {
      updateWeather();
  }

  // --- Animation Météo (à la minute 00) ---
  if (weatherEnabled && !fireworksMode && !chaserMode && !pixelTestMode && !weatherMode) {
      if (rtc.getMinute() == 0 && rtc.getSecond() == 0 && !weatherTriggeredThisHour) {
          weatherTriggeredThisHour = true;
          weatherMode = true;
          weatherStartTime = millis();
          clearLEDs(true);
          Serial.println("🌤️ Lancement de l'animation météo horaire");
      }
  }
  // Réarmement pour la prochaine heure
  if (rtc.getMinute() > 0) {
      weatherTriggeredThisHour = false;
  }

  // -------------------------------------------------------------------------
  //  GESTION DU BOUTON PHYSIQUE
  //  - Appui court (< 3s)  → Mode réglage luminosité
  //  - Appui long  (≥ 3s)  → Lancer le feu d'artifice
  //  - Pendant une animation → Un appui l'arrête
  // -------------------------------------------------------------------------
  int currentButtonState = digitalRead(btnSetting);
  static int lastButtonState = HIGH;
  static unsigned long buttonDownTime = 0;
  static bool buttonWasPressed = false;

  // Détection du front descendant (bouton enfoncé)
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonPressTime > debounceDelay) {
      buttonDownTime = millis();
      buttonWasPressed = true;
    }
  }
  // Détection du front montant (bouton relâché)
  else if (currentButtonState == HIGH && lastButtonState == LOW &&
           buttonWasPressed) {
    if (millis() - lastButtonPressTime > debounceDelay) {
      lastButtonPressTime = millis();
      unsigned long pressDuration = millis() - buttonDownTime;

      if (fireworksMode || chaserMode || pixelTestMode) {
        // Pendant une animation : un appui l'arrête
        fireworksMode = false;
        chaserMode = false;
        pixelTestMode = false;
        clearLEDs(true);
      } else {
        // Mode normal : interpréter la durée de l'appui
        if (pressDuration > 50 &&
            pressDuration < BRIGHTNESS_PRESS_DURATION_MS) {
          setLuminosity(); // Appui court → réglage luminosité
        } else if (pressDuration >= BRIGHTNESS_PRESS_DURATION_MS) {
          // Appui long → feu d'artifice
          Serial.println("Long press: Activating Fireworks!");
          fireworksMode = true;
          fireworksStartTime = millis();
          for (int i = 0; i < MAX_ROCKETS; ++i)
            rockets[i].active = false;
          for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i)
            particles[i].active = false;
          activeParticleCount = 0;
          clearLEDs(true);
        }
      }
      buttonWasPressed = false;
    }
  }
  lastButtonState = currentButtonState;

  // -------------------------------------------------------------------------
  //  AFFICHAGE — Sélection du mode actif
  // -------------------------------------------------------------------------
  if (fireworksMode) {
    // Animation feu d'artifice (durée limitée)
    if (millis() - fireworksStartTime < FIREWORKS_DURATION_MS) {
      runFireworksFrame();
      updateLEDs();
      FastLED.delay(33); // ~30 FPS (1000ms / 30)
    } else {
      fireworksMode = false;
      clearLEDs(true);
      Serial.println("Fireworks finished.");
    }
  } else if (weatherMode) {
    // Animation météo (15 secondes)
    if (millis() - weatherStartTime < 15000) {
      renderWeatherAnimation();
      FastLED.delay(50); // ~20 FPS
    } else {
      weatherMode = false;
      clearLEDs(true);
      Serial.println("Animation météo terminée.");
    }
  } else if (chaserMode) {
    runChaserFrame(); // Animation chenillard arc-en-ciel
  } else if (pixelTestMode) {
    runPixelTestFrame(); // Test LED une par une
  } else {
    // Mode horloge : rafraîchir l'affichage à intervalle régulier
    if (millis() - lastDisplayUpdateTime >= displayUpdateInterval) {
      lastDisplayUpdateTime = millis();
      clearLEDs(false);
      displayCurrentTime();
      updateLEDs();
    }
  }
}

// =====================================================================================
//  RÉGLAGE DE LA LUMINOSITÉ
//  Affiche un mot indicateur et attend des appuis bouton pour ajuster.
//  Sort automatiquement après 10 secondes d'inactivité.
// =====================================================================================
void setLuminosity() {
  Serial.println("--- Brightness Configuration Mode ---");
  clearLEDs(false);

  // Afficher le mot "LUMINOSITÉ" sur le ruban
  for (int i = 0; i < sizeof(displayBrightness_PGM) / sizeof(int); i++) {
    setPixelPair(pgm_read_word_near(displayBrightness_PGM + i),
                 CRGB::OrangeRed);
  }
  updateLEDs();

  unsigned long lastInteractionTime = millis();
  const unsigned long configTimeout =
      10000; // Timeout : 10 secondes d'inactivité
  int lastButtonState = digitalRead(btnSetting);

  while (millis() - lastInteractionTime < configTimeout) {
    int currentButtonState = digitalRead(btnSetting);

    // Détecter un appui bouton (front descendant)
    if (currentButtonState == LOW && lastButtonState == HIGH) {
      if (millis() - lastButtonPressTime > debounceDelay) {
        lastButtonPressTime = millis();
        lastInteractionTime = millis();

        // Incrémenter la luminosité par paliers de 20 (boucle 10 → 250)
        brightness += 20;
        if (brightness > 250)
          brightness = 10;
        if (brightness < 10)
          brightness = 10;

        // Sauvegarder et appliquer immédiatement
        preferences.putInt("brightness", brightness);
        Serial.print("Brightness: ");
        Serial.println(brightness);
        updateLEDs();
      }
    }
    lastButtonState = currentButtonState;
    delay(10);
  }
  Serial.println("Brightness set.");
  clearLEDs(true);
}

// =====================================================================================
//  AFFICHAGE DE L'HEURE
//
//  Logique d'affichage :
//    - À la minute 0 : phrase complète (ex: "IL EST TROIS HEURES")
//    - Minutes 1–59  : mot de l'heure seul avec effet de progression colorée
//                       (les lettres changent progressivement de couleur)
//
//  Cas spéciaux gérés séparément :
//    - 0h  (minuit)  → "IL EST MINUIT" / "MINUIT"
//    - 12h (midi)    → "IL EST MIDI"  / "MIDI" ou "MIAM" (alternance par jour)
//    - 16h (goûter)  → Phrase standard / "GOÛTER" (mot spécial)
// =====================================================================================
void displayCurrentTime() {
  int currentHour24 = rtc.getHour(true); // Format 24h (0–23)
  int currentMinute = rtc.getMinute();

  CRGB baseColor, progressColor;
  HourDisplayInfo currentHourConfigInfo;
  int configIndex;

  // ----- CAS SPÉCIAL : MINUIT (0h) -----
  if (currentHour24 == 0) {
    configIndex = 0;
    memcpy_P(&currentHourConfigInfo, &hourDisplayConfig_PGM[configIndex],
             sizeof(HourDisplayInfo));
    baseColor = displayColors[currentHourConfigInfo.colorSetIndex][0];
    progressColor = displayColors[currentHourConfigInfo.colorSetIndex][1];

    if (currentMinute == 0) {
      // "IL EST MINUIT"
      for (int i = 0; i < sizeof(midnightZero_PGM) / sizeof(int); i++)
        setPixelPair(pgm_read_word_near(midnightZero_PGM + i), baseColor);
    } else {
      // "MINUIT" seul avec progression colorée
      float advanceRatio = (float)currentMinute / 60.0f;
      int pixelsToProgress =
          round(advanceRatio * (sizeof(midnight_PGM) / sizeof(int)));
      for (int i = 0; i < (sizeof(midnight_PGM) / sizeof(int)); i++) {
        CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
        setPixelPair(pgm_read_word_near(midnight_PGM + i), color);
      }
    }
    return;
  }

  // ----- CAS SPÉCIAL : MIDI (12h) -----
  else if (currentHour24 == 12) {
    baseColor = displayColors[0][0];
    progressColor = displayColors[0][1];

    if (currentMinute == 0) {
      // "IL EST MIDI"
      for (int i = 0; i < sizeof(middayZero_PGM) / sizeof(int); i++)
        setPixelPair(pgm_read_word_near(middayZero_PGM + i), baseColor);
    } else {
      // Alternance "MIDI" / "MIAM" selon le jour (pair/impair)
      const int *wordToProgress_P =
          (rtc.getDay() % 2 == 0) ? midday_PGM : miam_PGM;
      int numWordPixels = (rtc.getDay() % 2 == 0)
                              ? sizeof(midday_PGM) / sizeof(int)
                              : sizeof(miam_PGM) / sizeof(int);
      if (numWordPixels > 0) {
        float advanceRatio = (float)currentMinute / 60.0f;
        int pixelsToProgress = round(advanceRatio * numWordPixels);
        for (int i = 0; i < numWordPixels; i++) {
          CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
          setPixelPair(pgm_read_word_near(wordToProgress_P + i), color);
        }
      }
    }
    return;
  }

  // ----- CAS SPÉCIAL : GOÛTER (16h) -----
  else if (currentHour24 == 16) {
    configIndex = 4; // Réutilise la config de 4h
    memcpy_P(&currentHourConfigInfo, &hourDisplayConfig_PGM[configIndex],
             sizeof(HourDisplayInfo));
    baseColor = displayColors[currentHourConfigInfo.colorSetIndex][0];
    progressColor = displayColors[currentHourConfigInfo.colorSetIndex][1];

    if (currentMinute == 0) {
      // Phrase complète de 4h (ex: "IL EST QUATRE HEURES")
      const int *pFullPhrase16 = (const int *)pgm_read_ptr_near(
          &currentHourConfigInfo.fullPhraseZeroMinute_P);
      for (int i = 0; i < currentHourConfigInfo.numFullPhraseZeroMinutePixels;
           i++) {
        setPixelPair(pgm_read_word_near(pFullPhrase16 + i), baseColor);
      }
    } else {
      // "GOÛTER" avec progression colorée
      float advanceRatio = (float)currentMinute / 60.0f;
      int pixelsToProgress =
          round(advanceRatio * (sizeof(gouter_PGM) / sizeof(int)));
      for (int i = 0; i < (sizeof(gouter_PGM) / sizeof(int)); i++) {
        CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
        setPixelPair(pgm_read_word_near(gouter_PGM + i), color);
      }
    }
    return;
  }

  // ----- CAS SPÉCIAL : APERO (18h) -----
  else if (currentHour24 == 18) {
    configIndex = 6; // 18h utilise config 6 (SIX HEURES)
    memcpy_P(&currentHourConfigInfo, &hourDisplayConfig_PGM[configIndex],
             sizeof(HourDisplayInfo));
    baseColor = displayColors[currentHourConfigInfo.colorSetIndex][0];
    progressColor = displayColors[currentHourConfigInfo.colorSetIndex][1];

    // APERO s'affiche 1 jour sur 2, ou le week-end (samedi=6, dimanche=0)
    // Note: rtc.getDayofWeek() renvoie 0 pour dimanche, 6 pour samedi
    bool isAperoDay = (rtc.getDay() % 2 == 0) || (rtc.getDayofWeek() == 0) ||
                      (rtc.getDayofWeek() == 6);

    if (currentMinute == 0) {
      // Phrase complète de 6h (ex: "IL EST SIX HEURES") à la minute 0
      const int *pFullPhrase18 = (const int *)pgm_read_ptr_near(
          &currentHourConfigInfo.fullPhraseZeroMinute_P);
      for (int i = 0; i < currentHourConfigInfo.numFullPhraseZeroMinutePixels;
           i++) {
        setPixelPair(pgm_read_word_near(pFullPhrase18 + i), baseColor);
      }
    } else {
      if (isAperoDay) {
        // "APERO" avec progression colorée
        float advanceRatio = (float)currentMinute / 60.0f;
        int pixelsToProgress =
            round(advanceRatio * (sizeof(apero_PGM) / sizeof(int)));
        for (int i = 0; i < (sizeof(apero_PGM) / sizeof(int)); i++) {
          CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
          setPixelPair(pgm_read_word_near(apero_PGM + i), color);
        }
      } else {
        // Comportement normal ("SIX HEURES") si ce n'est pas un jour APERO
        const int *pHourWordProgress18 = (const int *)pgm_read_ptr_near(
            &currentHourConfigInfo.hourWordProgress_P);
        float advanceRatio = (float)currentMinute / 60.0f;
        int pixelsToProgress = round(
            advanceRatio * currentHourConfigInfo.numHourWordProgressPixels);
        for (int i = 0; i < currentHourConfigInfo.numHourWordProgressPixels;
             i++) {
          CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
          setPixelPair(pgm_read_word_near(pHourWordProgress18 + i), color);
        }
      }
    }
    return;
  }

  // ----- CAS GÉNÉRAL (toutes les autres heures) -----
  configIndex = currentHour24 % 12;
  memcpy_P(&currentHourConfigInfo, &hourDisplayConfig_PGM[configIndex],
           sizeof(HourDisplayInfo));
  baseColor = displayColors[currentHourConfigInfo.colorSetIndex][0];
  progressColor = displayColors[currentHourConfigInfo.colorSetIndex][1];

  if (currentMinute == 0) {
    // Minute 0 : afficher la phrase complète (ex: "IL EST TROIS HEURES")
    const int *pFullPhrase = (const int *)pgm_read_ptr_near(
        &currentHourConfigInfo.fullPhraseZeroMinute_P);
    for (int i = 0; i < currentHourConfigInfo.numFullPhraseZeroMinutePixels;
         i++) {
      setPixelPair(pgm_read_word_near(pFullPhrase + i), baseColor);
    }
  } else {
    // Minutes 1–59 : mot de l'heure avec progression colorée
    const int *pHourWord = (const int *)pgm_read_ptr_near(
        &currentHourConfigInfo.hourWordProgress_P);
    float advanceRatio = (float)currentMinute / 60.0f;
    int pixelsToProgress =
        round(advanceRatio * currentHourConfigInfo.numHourWordProgressPixels);

    for (int i = 0; i < currentHourConfigInfo.numHourWordProgressPixels; i++) {
      CRGB color = (i < pixelsToProgress) ? progressColor : baseColor;
      setPixelPair(pgm_read_word_near(pHourWord + i), color);
    }
  }
}

// =====================================================================================
//  ANIMATIONS DE TEST
// =====================================================================================

/**
 * @brief Chenillard arc-en-ciel : un point lumineux parcourt le ruban avec une
 * traînée. Rafraîchissement : ~20 FPS (50ms par frame).
 */
void runChaserFrame() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 50) {
    lastUpdate = millis();
    fadeToBlackBy((CRGB*)&leds[0], (NUM_LEDS * 4) / 3,
                  50); // Estompage progressif → traînée lumineuse
    int pos = testPixelIndex % (NUM_LEDS / 2);
    setPixelPair(pos,
                 CHSV((millis() / 20) % 255, 255, 255)); // Couleur arc-en-ciel
    updateLEDs();
    testPixelIndex++;
  }
}

/**
 * @brief Test pixel par pixel : allume chaque paire une par une (blanc).
 * Utile pour vérifier le câblage et repérer les LEDs défectueuses.
 * Rafraîchissement : 2 FPS (500ms par frame).
 */
void runPixelTestFrame() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    clearLEDs(false);
    int pos = testPixelIndex % (NUM_LEDS / 2);
    setPixelPair(pos, CRGB::White);
    updateLEDs();
    testPixelIndex++;
  }
}
