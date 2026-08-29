#ifndef GLOBALS_H
#define GLOBALS_H

// =====================================================================================
//  GLOBALS.H — Déclarations globales, structures, fonctions utilitaires et données
//
//  Ce fichier est inclus par tous les autres modules (.ino, fireworks.h, web_setup.h).
//  Il contient :
//    - Les includes de bibliothèques
//    - Les structures de données (animations)
//    - Les déclarations extern des variables globales
//    - Les fonctions utilitaires inline (LEDs, mapping)
//    - Les tableaux de pixels pour chaque heure (PROGMEM)
//    - La table de configuration horaire et les couleurs par défaut
// =====================================================================================

#include "config.h"
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <ESP32Time.h>
#include <pgmspace.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =====================================================================================
//  STRUCTURES DE DONNÉES — Animation feu d'artifice
// =====================================================================================

// Représente une particule d'explosion (position, vitesse, couleur, durée de vie)
struct Particle {
    float x, y;        // Position sur la grille virtuelle
    float vx, vy;      // Vitesse (pixels par frame)
    CRGB color;         // Couleur RGB de la particule
    int lifespan;       // Nombre de frames restantes avant disparition
    bool active;        // true = particule visible et en mouvement
};

// Représente une fusée qui monte avant d'exploser
struct Rocket {
    float x, y;         // Position actuelle
    float target_y;     // Altitude cible où la fusée explose
    CHSV color;         // Couleur HSV (permet des variations de teinte faciles)
    bool active;         // true = fusée en vol
    int tail_length;     // Longueur de la traînée lumineuse
};

// =====================================================================================
//  DÉCLARATIONS DES VARIABLES GLOBALES (définies dans horlogeIA.ino)
// =====================================================================================

// --- Structure de support RGBW pour le hack SK6812 (Memory Punning) ---
struct CRGBW {
    union {
        struct { uint8_t g; uint8_t r; uint8_t b; uint8_t w; }; // Ordre GRBW en mémoire
        uint8_t raw[4];
    };
    CRGBW() {}
    CRGBW(uint8_t rd, uint8_t grn, uint8_t blu, uint8_t wht) {
        r = rd; g = grn; b = blu; w = wht;
    }
    CRGBW(const CRGB& rgb) {
        r = rgb.r; g = rgb.g; b = rgb.b; w = 0;
    }
    CRGBW& operator=(const CRGB& rhs) {
        r = rhs.r; g = rhs.g; b = rhs.b; w = 0;
        return *this;
    }
};

// --- Buffer LED principal ---
extern CRGBW leds[NUM_LEDS];

// --- Horloge temps réel et persistance
extern ESP32Time rtc;               // Horloge interne ESP32 (synchronisée via NTP)
extern Preferences preferences;     // Stockage persistant en flash (Wi-Fi, couleurs, luminosité)
extern WebServer server;            // Serveur web pour la configuration à distance

// --- Drapeaux d'état (modes d'affichage)
extern bool apModeActive;           // true = mode Point d'Accès Wi-Fi (configuration)
extern bool fireworksMode;          // true = animation feu d'artifice en cours
extern bool chaserMode;             // true = animation chenillard en cours
extern bool pixelTestMode;          // true = test LED une par une en cours
extern bool weatherMode;            // true = animation météo en cours
extern int testPixelIndex;          // Index courant pour les animations de test

// --- Drapeaux d'état (Météo)
extern bool weatherEnabled;
extern String weatherApiKey;
extern String weatherCity;
extern int currentWeatherId;
extern unsigned long lastWeatherSync;
extern unsigned long weatherStartTime;
extern bool weatherTriggeredThisHour;

// --- Drapeaux d'état (Anniversaires)
extern bool isBirthdayToday;
extern int birthdayFireworkMinute;
extern bool fireworkTriggeredThisHour;
extern int lastHourChecked;
extern int lastDayChecked;

extern unsigned long lastNtpSync;   // Timestamp de la dernière synchronisation NTP
extern unsigned long fireworksStartTime; // Timestamp du début du feu d'artifice

// --- Bouton et variables dynamiques
extern unsigned long lastButtonPressTime;    // Anti-rebond : dernier appui détecté
extern int activeParticleCount;              // Nombre de particules actives (feu d'artifice)
extern unsigned long lastDisplayUpdateTime;  // Dernier rafraîchissement de l'affichage horloge
extern int brightness;                       // Luminosité globale (10–250)

// --- Tableaux d'animation
extern Rocket rockets[MAX_ROCKETS];
extern Particle particles[MAX_TOTAL_PARTICLES];
extern CRGB displayColors[12][2];  // [heure][0=base, 1=progression] — couleurs configurables

// =====================================================================================
//  FONCTIONS UTILITAIRES INLINE
// =====================================================================================

/**
 * @brief Applique la luminosité et envoie le buffer LED au ruban.
 * FastLED gère nativement le 4ème octet (blanc) des SK6812 via setRgbw().
 */
inline void updateLEDs() {
    FastLED.setBrightness(brightness);
    FastLED.show();
}

/**
 * @brief Éteint toutes les LEDs. Optionnellement envoie la mise à jour au ruban.
 * @param show  Si true, appelle updateLEDs() immédiatement.
 */
inline void clearLEDs(bool show = false) {
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
    if (show) updateLEDs();
}

/**
 * @brief Allume une paire logique de LEDs (2 LEDs physiques côte à côte).
 *
 * Le ruban est câblé en paires : la LED logique N correspond aux LEDs
 * physiques 2*N et 2*N+1. Cette fonction applique la même couleur aux deux.
 *
 * @param index  Index logique de la paire (0 à NUM_LEDS/2 - 1)
 * @param color  Couleur RGB à appliquer
 */
inline void setPixelPair(int index, CRGB color) {
    if (index >= 0 && index * 2 + 1 < NUM_LEDS) {
        leds[index * 2] = color;
        leds[index * 2 + 1] = color;
    }
}

/**
 * @brief Convertit des coordonnées (x, y) en index de paire logique.
 *
 * Le ruban est disposé en serpentin (zigzag) : les lignes paires vont de
 * gauche à droite, les lignes impaires de droite à gauche.
 *
 * @param x  Colonne (0 à GRID_WIDTH-1)
 * @param y  Ligne   (0 à GRID_HEIGHT-1)
 * @return   Index de la paire logique, ou -1 si hors limites
 */
inline int xy_map(int x, int y) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) {
        return -1;
    }
    int index;
    if (y % 2 == 0) {
        index = y * GRID_WIDTH + x;               // Ligne paire : gauche → droite
    } else {
        index = y * GRID_WIDTH + (GRID_WIDTH - 1 - x); // Ligne impaire : droite → gauche
    }
    if (index >= 0 && index < (GRID_WIDTH * GRID_HEIGHT)) {
        return index;
    }
    return -1;
}

// =====================================================================================
//  TABLEAUX DE PIXELS — Positions des LEDs pour chaque mot (PROGMEM)
//
//  Convention de nommage :
//    xxxZero_PGM  = Phrase complète affichée à la minute 0 (ex: "IL EST UNE HEURE")
//    xxx_PGM      = Mot de l'heure seul, utilisé pour la progression (ex: "UNE")
//
//  Les valeurs sont des indices de paires logiques sur le ruban.
//  Stockés en PROGMEM pour économiser la RAM de l'ESP32.
// =====================================================================================

// --- Heures standards (1h–11h) ---
const int oneZero_PGM[13]    PROGMEM = {0, 1, 19, 20, 21, 38, 39, 40, 97, 98, 99, 100, 101};             // "IL EST UNE HEURE"
const int one_PGM[8]         PROGMEM = {40, 39, 38, 97, 98, 99, 100, 101};                                // "UNE HEURE"
const int twoZero_PGM[15]   PROGMEM = {0, 1, 3, 20, 27, 31, 32, 33, 34, 66, 67, 68, 69, 70, 71};        // "IL EST DEUX HEURES"
const int two_PGM[10]        PROGMEM = {31, 32, 33, 34, 71, 70, 69, 68, 67, 66};                          // "DEUX HEURES"
const int treeZero_PGM[16]  PROGMEM = {45, 46, 65, 66, 67, 115, 116, 117, 118, 119, 133, 134, 135, 136, 137, 138}; // "IL EST TROIS HEURES"
const int tree_PGM[11]       PROGMEM = {119, 118, 117, 116, 115, 138, 137, 136, 135, 134, 133};           // "TROIS HEURES"
const int fourZero_PGM[17]  PROGMEM = {24, 25, 26, 27, 28, 29, 54, 55, 56, 57, 58, 59, 94, 95, 101, 102, 103}; // "IL EST QUATRE HEURES"
const int four_PGM[12]       PROGMEM = {24, 25, 26, 27, 28, 29, 54, 55, 56, 57, 58, 59};                 // "QUATRE HEURES"
const int fiveZero_PGM[15]  PROGMEM = {0, 1, 12, 13, 14, 15, 19, 20, 21, 66, 67, 68, 69, 70, 71};       // "IL EST CINQ HEURES"
const int five_PGM[10]       PROGMEM = {15, 14, 13, 12, 71, 70, 69, 68, 67, 66};                          // "CINQ HEURES"
const int sixZero_PGM[14]   PROGMEM = {0, 1, 19, 20, 21, 50, 51, 52, 54, 55, 56, 57, 58, 59};            // "IL EST SIX HEURES"
const int six_PGM[9]         PROGMEM = {50, 51, 52, 54, 55, 56, 57, 58, 59};                              // "SIX HEURES"
const int sevenZero_PGM[15] PROGMEM = {45, 46, 65, 66, 67, 80, 81, 82, 83, 97, 98, 99, 100, 101, 102};   // "IL EST SEPT HEURES"
const int seven_PGM[10]      PROGMEM = {80, 81, 82, 83, 97, 98, 99, 100, 101, 102};                       // "SEPT HEURES"
const int eightZero_PGM[15] PROGMEM = {45, 46, 65, 66, 67, 84, 85, 86, 87, 97, 98, 99, 100, 101, 102};   // "IL EST HUIT HEURES"
const int eight_PGM[10]      PROGMEM = {87, 86, 85, 84, 97, 98, 99, 100, 101, 102};                       // "HUIT HEURES"
const int nineZero_PGM[15]  PROGMEM = {94, 95, 101, 102, 103, 121, 122, 123, 124, 133, 134, 135, 136, 137, 138}; // "IL EST NEUF HEURES"
const int nine_PGM[10]       PROGMEM = {121, 122, 123, 124, 138, 137, 136, 135, 134, 133};                // "NEUF HEURES"
const int tenZero_PGM[14]   PROGMEM = {0, 1, 41, 42, 43, 74, 75, 76, 108, 109, 110, 111, 112, 113};      // "IL EST DIX HEURES"
const int ten_PGM[9]         PROGMEM = {74, 75, 76, 113, 112, 111, 110, 109, 108};                        // "DIX HEURES"
const int elevenZero_PGM[15] PROGMEM = {45, 46, 65, 66, 67, 108, 109, 110, 111, 112, 113, 143, 142, 141, 140}; // "IL EST ONZE HEURES"
const int eleven_PGM[10]    PROGMEM = {143, 142, 141, 140, 113, 112, 111, 110, 109, 108};                 // "ONZE HEURES"

// --- Cas spéciaux : Midi, Minuit, Goûter, Miam ---
const int middayZero_PGM[9]    PROGMEM = {45, 46, 60, 61, 62, 63, 65, 66, 67};            // "IL EST MIDI"
const int midday_PGM[4]        PROGMEM = {63, 62, 61, 60};                                 // "MIDI"
const int miam_PGM[4]          PROGMEM = {104, 105, 106, 107};                              // "MIAM" (easter egg)
const int midnightZero_PGM[11] PROGMEM = {45, 46, 65, 66, 67, 88, 89, 90, 91, 92, 93};    // "IL EST MINUIT"
const int midnight_PGM[6]      PROGMEM = {93, 92, 91, 90, 89, 88};                         // "MINUIT"
const int gouter_PGM[6]        PROGMEM = {125, 126, 127, 128, 129, 130};                    // "GOÛTER" (affiché à 16h)
const int apero_PGM[5]         PROGMEM = {26, 49, 67, 100, 126};                            // "APERO" (affiché à 18h selon les jours)

// --- Mot affiché pendant le réglage de luminosité ---
const int displayBrightness_PGM[17] PROGMEM = {16, 17, 18, 30, 48, 49, 53, 72, 73, 78, 79, 106, 107, 108, 109, 110, 111};

// =====================================================================================
//  STRUCTURE DE CONFIGURATION HORAIRE
// =====================================================================================

/**
 * @brief Configuration d'affichage pour une heure donnée.
 *
 * Chaque entrée lie une heure (0–11) à :
 *   - La phrase complète affichée à la minute 0
 *   - Le mot de l'heure seul pour la progression des minutes
 *   - L'index du jeu de couleurs associé
 */
struct HourDisplayInfo {
    const int *fullPhraseZeroMinute_P;      // Pointeur PROGMEM vers la phrase complète (ex: "IL EST UNE HEURE")
    uint8_t numFullPhraseZeroMinutePixels;  // Nombre de paires dans la phrase complète
    const int *hourWordProgress_P;          // Pointeur PROGMEM vers le mot seul (ex: "UNE")
    uint8_t numHourWordProgressPixels;      // Nombre de paires dans le mot seul
    uint8_t colorSetIndex;                  // Index dans displayColors[12][2]
};

// Table de configuration pour les 12 heures (0=minuit/midi, 1=1h/13h, ..., 11=11h/23h)
const HourDisplayInfo hourDisplayConfig_PGM[12] PROGMEM = {
    {midnightZero_PGM, sizeof(midnightZero_PGM) / sizeof(int), midnight_PGM, sizeof(midnight_PGM) / sizeof(int), 0},
    {oneZero_PGM,      sizeof(oneZero_PGM)      / sizeof(int), one_PGM,      sizeof(one_PGM)      / sizeof(int), 1},
    {twoZero_PGM,      sizeof(twoZero_PGM)      / sizeof(int), two_PGM,      sizeof(two_PGM)      / sizeof(int), 2},
    {treeZero_PGM,     sizeof(treeZero_PGM)     / sizeof(int), tree_PGM,     sizeof(tree_PGM)     / sizeof(int), 3},
    {fourZero_PGM,     sizeof(fourZero_PGM)     / sizeof(int), four_PGM,     sizeof(four_PGM)     / sizeof(int), 4},
    {fiveZero_PGM,     sizeof(fiveZero_PGM)     / sizeof(int), five_PGM,     sizeof(five_PGM)     / sizeof(int), 5},
    {sixZero_PGM,      sizeof(sixZero_PGM)      / sizeof(int), six_PGM,      sizeof(six_PGM)      / sizeof(int), 6},
    {sevenZero_PGM,    sizeof(sevenZero_PGM)    / sizeof(int), seven_PGM,    sizeof(seven_PGM)    / sizeof(int), 7},
    {eightZero_PGM,    sizeof(eightZero_PGM)    / sizeof(int), eight_PGM,    sizeof(eight_PGM)    / sizeof(int), 8},
    {nineZero_PGM,     sizeof(nineZero_PGM)     / sizeof(int), nine_PGM,     sizeof(nine_PGM)     / sizeof(int), 9},
    {tenZero_PGM,      sizeof(tenZero_PGM)      / sizeof(int), ten_PGM,      sizeof(ten_PGM)      / sizeof(int), 10},
    {elevenZero_PGM,   sizeof(elevenZero_PGM)   / sizeof(int), eleven_PGM,   sizeof(eleven_PGM)   / sizeof(int), 11}
};

// =====================================================================================
//  COULEURS PAR DÉFAUT (PROGMEM)
//  Chaque heure a deux couleurs : une pour le texte de base, une pour la progression.
//  Ces valeurs sont utilisées si l'utilisateur n'a pas personnalisé via l'interface web.
// =====================================================================================
const CRGB defaultColors_PGM[12][2] PROGMEM = {
    // {Couleur de base (phrase/mot)},        {Couleur de progression (minutes)}
    {CRGB(255, 192, 203), CRGB(173, 216, 230)}, //  0. Rose Pâle — Bleu Clair
    {CRGB(220, 180, 255), CRGB(255, 248, 170)}, //  1. Lavande Douce — Jaune Crème
    {CRGB(135, 206, 250), CRGB(255, 180, 150)}, //  2. Bleu Ciel — Corail Doux
    {CRGB(150, 230, 180), CRGB(240, 180, 190)}, //  3. Vert Menthe — Vieux Rose
    {CRGB(255, 160, 122), CRGB(100, 149, 237)}, //  4. Saumon — Bleuet
    {CRGB(153, 153, 255), CRGB(245, 245, 220)}, //  5. Pervenche — Beige Clair
    {CRGB(218, 112, 214), CRGB(144, 238, 144)}, //  6. Orchidée — Vert Clair
    {CRGB(240, 230, 140), CRGB(120,  81, 169)}, //  7. Kaki — Améthyste
    {CRGB(175, 238, 238), CRGB(255, 165, 180)}, //  8. Turquoise Pâle — Rose Corail
    {CRGB( 70, 130, 180), CRGB(255, 215,   0)}, //  9. Bleu Acier — Or
    {CRGB(255, 222, 173), CRGB(147, 112, 219)}, // 10. Pêche — Violet Moyen
    {CRGB(152, 251, 152), CRGB(221, 160, 221)}  // 11. Vert Pâle — Prune
};

#endif // GLOBALS_H
