#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================================
//  FICHIER DE CONFIGURATION
//  Ce fichier centralise toutes les constantes matérielles et les paramètres
//  ajustables du projet. Modifier les valeurs ici pour adapter le projet
//  à votre montage sans toucher au code principal.
// =====================================================================================

// --- LEDs ---
#define LED_PIN     10        // Broche GPIO de données du ruban LED
#define CHIPSET     SK6812    // Type de LEDs (SK6812 = RGBW natif)
#define NUM_LEDS    288       // Nombre total de LEDs sur le ruban (144 paires logiques × 2)

// --- Synchronisation NTP ---
const char* ntpServer = "pool.ntp.org";
const unsigned long ntpSyncInterval = 24UL * 60 * 60 * 1000; // Resynchronisation toutes les 24h (en ms)

// --- Point d'Accès Wi-Fi (mode configuration) ---
const char* ssid     = "Clock-Setup";   // Nom du réseau Wi-Fi créé par l'horloge
const char* password = NULL;            // Pas de mot de passe pour faciliter la connexion initiale

// --- Bouton physique ---
const int btnSetting = 6;               // Broche GPIO du bouton de réglage
const unsigned long debounceDelay = 50;  // Anti-rebond matériel (en ms)

// --- Durées d'appui pour les interactions ---
const unsigned long BRIGHTNESS_PRESS_DURATION_MS = 3000;  // Appui court → réglage luminosité
const unsigned long FIREWORKS_PRESS_DURATION_MS  = 5000;  // Appui long  → feu d'artifice

// --- Horloge ---
const unsigned long displayUpdateInterval = 1000; // Rafraîchissement de l'affichage (en ms)

// --- Animation Feu d'Artifice ---
#define GRID_WIDTH  12        // Largeur de la grille virtuelle (colonnes)
#define GRID_HEIGHT 12        // Hauteur de la grille virtuelle (lignes)

#define MAX_ROCKETS                 2   // Nombre max de fusées simultanées
#define MAX_PARTICLES_PER_EXPLOSION 5  // Particules par explosion
#define MAX_TOTAL_PARTICLES (MAX_ROCKETS * MAX_PARTICLES_PER_EXPLOSION)

const unsigned long FIREWORKS_DURATION_MS = 30000; // Durée de l'animation (30 secondes)

#endif // CONFIG_H
