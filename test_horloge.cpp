// =====================================================================================
//  TEST UNITAIRE — HorlogeIA
//
//  Teste les fonctions de logique pure du projet sans matériel Arduino :
//    - xy_map()       : conversion coordonnées grille → index LED
//    - setPixelPair() : écriture par paire avec vérification des bornes
//    - Sélection de l'index de configuration horaire (cas spéciaux + général)
//    - Encodage/décodage des couleurs uint32 ↔ RGB
//    - Logique de luminosité (clamping, wrapping)
//
//  Compilation : clang++ -std=c++17 -o test_horloge test_horloge.cpp &&
//  ./test_horloge
// =====================================================================================
#ifndef ARDUINO

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// =====================================================================================
//  MOCKS — Simule les types Arduino/FastLED pour permettre la compilation
//  desktop
// =====================================================================================

// Mock CRGB
struct CRGB {
  uint8_t r, g, b;
  CRGB() : r(0), g(0), b(0) {}
  CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  bool operator==(const CRGB &o) const {
    return r == o.r && g == o.g && b == o.b;
  }
  bool operator!=(const CRGB &o) const { return !(*this == o); }
  static const CRGB Black;
};
const CRGB CRGB::Black = CRGB(0, 0, 0);

// Mock PROGMEM (sur desktop, c'est juste de la RAM normale)
#define PROGMEM
#define pgm_read_word_near(addr) (*(const int *)(addr))

// =====================================================================================
//  CONSTANTES — Reprises de config.h
// =====================================================================================
#define GRID_WIDTH 12
#define GRID_HEIGHT 12
#define NUM_LEDS 288

// =====================================================================================
//  FONCTIONS SOUS TEST — Copiées depuis globals.h
// =====================================================================================

// Buffer LED global (mock)
CRGB leds[NUM_LEDS];

inline int xy_map(int x, int y) {
  if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) {
    return -1;
  }
  int index;
  if (y % 2 == 0) {
    index = y * GRID_WIDTH + x;
  } else {
    index = y * GRID_WIDTH + (GRID_WIDTH - 1 - x);
  }
  if (index >= 0 && index < (GRID_WIDTH * GRID_HEIGHT)) {
    return index;
  }
  return -1;
}

inline void setPixelPair(int index, CRGB color) {
  if (index >= 0 && index * 2 + 1 < NUM_LEDS) {
    leds[index * 2] = color;
    leds[index * 2 + 1] = color;
  }
}

inline void clearLEDs() {
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Black;
}

// =====================================================================================
//  FRAMEWORK DE TEST MINIMAL
// =====================================================================================

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void name()
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  %-55s", #name);                                                  \
    tests_run++;                                                               \
    try {                                                                      \
      name();                                                                  \
      tests_passed++;                                                          \
      printf("✅ PASS\n");                                                     \
    } catch (...) {                                                            \
      tests_failed++;                                                          \
      printf("❌ FAIL\n");                                                     \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    if (_a != _b) {                                                            \
      printf("\n    ASSERT_EQ failed: %s == %d, expected %d\n", #a, (int)_a,   \
             (int)_b);                                                         \
      throw 1;                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("\n    ASSERT_TRUE failed: %s\n", #cond);                         \
      throw 1;                                                                 \
    }                                                                          \
  } while (0)

// =====================================================================================
//  TESTS — xy_map()
// =====================================================================================

TEST(test_xy_map_origin) {
  // Coin supérieur gauche (0,0) → index 0
  ASSERT_EQ(xy_map(0, 0), 0);
}

TEST(test_xy_map_even_row) {
  // Ligne paire (y=0) : gauche → droite
  ASSERT_EQ(xy_map(0, 0), 0);
  ASSERT_EQ(xy_map(1, 0), 1);
  ASSERT_EQ(xy_map(11, 0), 11);
}

TEST(test_xy_map_odd_row) {
  // Ligne impaire (y=1) : droite → gauche (serpentin)
  ASSERT_EQ(xy_map(0, 1), 12 + 11); // x=0 sur y=1 → index 23
  ASSERT_EQ(xy_map(11, 1), 12);     // x=11 sur y=1 → index 12
  ASSERT_EQ(xy_map(5, 1), 12 + 6);  // x=5 sur y=1 → index 18
}

TEST(test_xy_map_last_pixel) {
  // y=10 est une ligne paire → gauche à droite → index = 10*12 + 11 = 131
  ASSERT_EQ(xy_map(11, 10), 131);
  // Dernier pixel absolu de la grille : (11, 11) → ligne impaire → 11*12 +
  // (11-11) = 132
  ASSERT_EQ(xy_map(11, 11), 132);
}

TEST(test_xy_map_out_of_bounds_negative) {
  ASSERT_EQ(xy_map(-1, 0), -1);
  ASSERT_EQ(xy_map(0, -1), -1);
  ASSERT_EQ(xy_map(-5, -5), -1);
}

TEST(test_xy_map_out_of_bounds_too_large) {
  ASSERT_EQ(xy_map(12, 0), -1); // x >= GRID_WIDTH
  ASSERT_EQ(xy_map(0, 12), -1); // y >= GRID_HEIGHT
  ASSERT_EQ(xy_map(100, 100), -1);
}

TEST(test_xy_map_serpentine_consistency) {
  // Vérifier que chaque position de la grille donne un index unique et valide
  bool used[GRID_WIDTH * GRID_HEIGHT] = {false};
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      int idx = xy_map(x, y);
      ASSERT_TRUE(idx >= 0);
      ASSERT_TRUE(idx < GRID_WIDTH * GRID_HEIGHT);
      ASSERT_TRUE(!used[idx]); // Chaque index ne doit apparaître qu'une fois
      used[idx] = true;
    }
  }
  // Vérifier que tous les indices ont été utilisés
  for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
    ASSERT_TRUE(used[i]);
  }
}

// =====================================================================================
//  TESTS — setPixelPair()
// =====================================================================================

TEST(test_setPixelPair_basic) {
  clearLEDs();
  CRGB red(255, 0, 0);
  setPixelPair(0, red);
  // Paire 0 = LEDs physiques 0 et 1
  ASSERT_TRUE(leds[0] == red);
  ASSERT_TRUE(leds[1] == red);
  // LED 2 ne doit pas être touchée
  ASSERT_TRUE(leds[2] == CRGB::Black);
}

TEST(test_setPixelPair_middle) {
  clearLEDs();
  CRGB blue(0, 0, 255);
  setPixelPair(50, blue);
  // Paire 50 = LEDs physiques 100 et 101
  ASSERT_TRUE(leds[100] == blue);
  ASSERT_TRUE(leds[101] == blue);
  // LEDs adjacentes non touchées
  ASSERT_TRUE(leds[99] == CRGB::Black);
  ASSERT_TRUE(leds[102] == CRGB::Black);
}

TEST(test_setPixelPair_last_valid) {
  clearLEDs();
  CRGB green(0, 255, 0);
  // Dernière paire valide : index 143 → LEDs 286 et 287
  setPixelPair(143, green);
  ASSERT_TRUE(leds[286] == green);
  ASSERT_TRUE(leds[287] == green);
}

TEST(test_setPixelPair_out_of_bounds) {
  clearLEDs();
  CRGB white(255, 255, 255);
  // Index négatif : ne doit rien modifier
  setPixelPair(-1, white);
  for (int i = 0; i < NUM_LEDS; i++) {
    ASSERT_TRUE(leds[i] == CRGB::Black);
  }
  // Index trop grand : ne doit rien modifier
  setPixelPair(144, white);
  for (int i = 0; i < NUM_LEDS; i++) {
    ASSERT_TRUE(leds[i] == CRGB::Black);
  }
}

TEST(test_setPixelPair_overwrite) {
  clearLEDs();
  CRGB red(255, 0, 0);
  CRGB blue(0, 0, 255);
  setPixelPair(10, red);
  ASSERT_TRUE(leds[20] == red);
  // Écraser avec une autre couleur
  setPixelPair(10, blue);
  ASSERT_TRUE(leds[20] == blue);
  ASSERT_TRUE(leds[21] == blue);
}

// =====================================================================================
//  TESTS — Sélection de l'index de configuration horaire
// =====================================================================================

// Reproduit la logique de displayCurrentTime() pour le choix du configIndex
int getConfigIndex(int hour24) {
  if (hour24 == 0)
    return 0; // Minuit → config 0
  if (hour24 == 12)
    return 0; // Midi   → config 0 (couleurs partagées)
  if (hour24 == 16)
    return 4;         // Goûter → config 4 (même que 4h)
  return hour24 % 12; // Cas général
}

// Reproduit le choix du "mot spécial" vs cas général
enum HourDisplayMode { MIDNIGHT, MIDDAY, GOUTER, STANDARD };
HourDisplayMode getDisplayMode(int hour24) {
  if (hour24 == 0)
    return MIDNIGHT;
  if (hour24 == 12)
    return MIDDAY;
  if (hour24 == 16)
    return GOUTER;
  return STANDARD;
}

TEST(test_config_index_midnight) {
  ASSERT_EQ(getConfigIndex(0), 0);
  ASSERT_EQ(getDisplayMode(0), MIDNIGHT);
}

TEST(test_config_index_midday) {
  ASSERT_EQ(getConfigIndex(12), 0);
  ASSERT_EQ(getDisplayMode(12), MIDDAY);
}

TEST(test_config_index_gouter) {
  ASSERT_EQ(getConfigIndex(16), 4);
  ASSERT_EQ(getDisplayMode(16), GOUTER);
}

TEST(test_config_index_standard_am) {
  // Heures du matin (1h–11h)
  for (int h = 1; h <= 11; h++) {
    ASSERT_EQ(getConfigIndex(h), h);
    ASSERT_EQ(getDisplayMode(h), STANDARD);
  }
}

TEST(test_config_index_standard_pm) {
  // Heures de l'après-midi (13h–15h, 17h–23h)
  // 13h → config 1, 14h → config 2, etc.
  ASSERT_EQ(getConfigIndex(13), 1);
  ASSERT_EQ(getConfigIndex(14), 2);
  ASSERT_EQ(getConfigIndex(15), 3);
  ASSERT_EQ(getConfigIndex(17), 5);
  ASSERT_EQ(getConfigIndex(18), 6);
  ASSERT_EQ(getConfigIndex(19), 7);
  ASSERT_EQ(getConfigIndex(20), 8);
  ASSERT_EQ(getConfigIndex(21), 9);
  ASSERT_EQ(getConfigIndex(22), 10);
  ASSERT_EQ(getConfigIndex(23), 11);
}

// =====================================================================================
//  TESTS — Encodage/décodage couleurs uint32 ↔ RGB
// =====================================================================================

uint32_t crgbToUint32(CRGB c) {
  return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

CRGB uint32ToCrgb(uint32_t val) {
  return CRGB((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
}

TEST(test_color_encode_pure_red) {
  CRGB red(255, 0, 0);
  ASSERT_EQ(crgbToUint32(red), 0xFF0000u);
}

TEST(test_color_encode_pure_green) {
  CRGB green(0, 255, 0);
  ASSERT_EQ(crgbToUint32(green), 0x00FF00u);
}

TEST(test_color_encode_pure_blue) {
  CRGB blue(0, 0, 255);
  ASSERT_EQ(crgbToUint32(blue), 0x0000FFu);
}

TEST(test_color_encode_white) {
  CRGB white(255, 255, 255);
  ASSERT_EQ(crgbToUint32(white), 0xFFFFFFu);
}

TEST(test_color_encode_black) {
  CRGB black(0, 0, 0);
  ASSERT_EQ(crgbToUint32(black), 0x000000u);
}

TEST(test_color_roundtrip) {
  // Encoder puis décoder doit redonner la même couleur
  CRGB original(173, 216, 230); // Bleu clair (couleur du projet)
  uint32_t encoded = crgbToUint32(original);
  CRGB decoded = uint32ToCrgb(encoded);
  ASSERT_TRUE(original == decoded);
}

TEST(test_color_roundtrip_all_defaults) {
  // Tester avec toutes les couleurs par défaut du projet
  CRGB defaults[][2] = {
      {CRGB(255, 192, 203), CRGB(173, 216, 230)},
      {CRGB(220, 180, 255), CRGB(255, 248, 170)},
      {CRGB(135, 206, 250), CRGB(255, 180, 150)},
      {CRGB(150, 230, 180), CRGB(240, 180, 190)},
      {CRGB(255, 160, 122), CRGB(100, 149, 237)},
      {CRGB(153, 153, 255), CRGB(245, 245, 220)},
  };
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 2; j++) {
      uint32_t enc = crgbToUint32(defaults[i][j]);
      CRGB dec = uint32ToCrgb(enc);
      ASSERT_TRUE(defaults[i][j] == dec);
    }
  }
}

// =====================================================================================
//  TESTS — Logique de luminosité
// =====================================================================================

// Reproduit la logique de setLuminosity()
int adjustBrightness(int current) {
  current += 20;
  if (current > 250)
    current = 10;
  if (current < 10)
    current = 10;
  return current;
}

TEST(test_brightness_increment) {
  ASSERT_EQ(adjustBrightness(150), 170);
  ASSERT_EQ(adjustBrightness(100), 120);
}

TEST(test_brightness_wrap_around) {
  // 250 + 20 = 270 → wraps to 10
  ASSERT_EQ(adjustBrightness(250), 10);
  // 240 + 20 = 260 → wraps to 10
  ASSERT_EQ(adjustBrightness(240), 10);
}

TEST(test_brightness_min_value) {
  // La valeur minimale est 10
  ASSERT_EQ(adjustBrightness(250), 10);
}

TEST(test_brightness_full_cycle) {
  // Parcourir un cycle complet de luminosité
  int b = 10;
  int steps = 0;
  do {
    b = adjustBrightness(b);
    steps++;
  } while (b != 10 && steps < 100);
  // Doit revenir à 10 après un nombre fini d'étapes
  ASSERT_EQ(b, 10);
  // 10 → 30 → 50 → ... → 250 → 10 = (250-10)/20 + 1 = 13 étapes
  ASSERT_EQ(steps, 13);
}

// =====================================================================================
//  TESTS — Progression des minutes (calcul du ratio)
// =====================================================================================

TEST(test_progress_minute_0) {
  float ratio = 0.0f / 60.0f;
  int numPixels = 6; // ex: "MINUIT" = 6 lettres
  int pixelsToProgress = round(ratio * numPixels);
  ASSERT_EQ(pixelsToProgress, 0);
}

TEST(test_progress_minute_30) {
  float ratio = 30.0f / 60.0f; // 50%
  int numPixels = 6;
  int pixelsToProgress = round(ratio * numPixels);
  ASSERT_EQ(pixelsToProgress, 3); // 50% de 6 = 3
}

TEST(test_progress_minute_59) {
  float ratio = 59.0f / 60.0f; // ~98%
  int numPixels = 6;
  int pixelsToProgress = round(ratio * numPixels);
  ASSERT_EQ(pixelsToProgress, 6); // Presque tout allumé
}

TEST(test_progress_4_letters) {
  // "MIDI" = 4 lettres
  float ratio = 15.0f / 60.0f; // 25%
  int numPixels = 4;
  int pixelsToProgress = round(ratio * numPixels);
  ASSERT_EQ(pixelsToProgress, 1); // 25% de 4 = 1
}

// =====================================================================================
//  TESTS — Tableaux PROGMEM (tailles et cohérence)
// =====================================================================================

// Reproduit les tableaux PROGMEM pour vérifier les tailles
const int midnightZero_PGM[11] PROGMEM = {45, 46, 65, 66, 67, 88,
                                          89, 90, 91, 92, 93};
const int midnight_PGM[6] PROGMEM = {93, 92, 91, 90, 89, 88};
const int middayZero_PGM[9] PROGMEM = {45, 46, 60, 61, 62, 63, 65, 66, 67};
const int midday_PGM[4] PROGMEM = {63, 62, 61, 60};

TEST(test_progmem_sizes) {
  // Vérifier que les tailles déclarées correspondent
  ASSERT_EQ(sizeof(midnightZero_PGM) / sizeof(int), 11u);
  ASSERT_EQ(sizeof(midnight_PGM) / sizeof(int), 6u);
  ASSERT_EQ(sizeof(middayZero_PGM) / sizeof(int), 9u);
  ASSERT_EQ(sizeof(midday_PGM) / sizeof(int), 4u);
}

TEST(test_progmem_indices_in_bounds) {
  // Tous les indices de pixels doivent être < NUM_LEDS/2 (= 144 paires)
  int maxPair = NUM_LEDS / 2;

  for (int i = 0; i < 11; i++) {
    int idx = pgm_read_word_near(midnightZero_PGM + i);
    ASSERT_TRUE(idx >= 0);
    ASSERT_TRUE(idx < maxPair);
  }
  for (int i = 0; i < 6; i++) {
    int idx = pgm_read_word_near(midnight_PGM + i);
    ASSERT_TRUE(idx >= 0);
    ASSERT_TRUE(idx < maxPair);
  }
}

TEST(test_midnight_word_subset_of_phrase) {
  // Les indices de "MINUIT" doivent tous apparaître dans "IL EST MINUIT"
  for (int i = 0; i < 6; i++) {
    int wordIdx = pgm_read_word_near(midnight_PGM + i);
    bool found = false;
    for (int j = 0; j < 11; j++) {
      if (pgm_read_word_near(midnightZero_PGM + j) == wordIdx) {
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }
}

// =====================================================================================
//  MAIN — Exécution de tous les tests
// =====================================================================================

int main() {
  printf("\n🧪 HorlogeIA — Tests Unitaires\n");
  printf(
      "════════════════════════════════════════════════════════════════\n\n");

  printf("📐 xy_map() — Mapping grille → index LED\n");
  RUN_TEST(test_xy_map_origin);
  RUN_TEST(test_xy_map_even_row);
  RUN_TEST(test_xy_map_odd_row);
  RUN_TEST(test_xy_map_last_pixel);
  RUN_TEST(test_xy_map_out_of_bounds_negative);
  RUN_TEST(test_xy_map_out_of_bounds_too_large);
  RUN_TEST(test_xy_map_serpentine_consistency);

  printf("\n💡 setPixelPair() — Écriture par paire\n");
  RUN_TEST(test_setPixelPair_basic);
  RUN_TEST(test_setPixelPair_middle);
  RUN_TEST(test_setPixelPair_last_valid);
  RUN_TEST(test_setPixelPair_out_of_bounds);
  RUN_TEST(test_setPixelPair_overwrite);

  printf("\n🕐 Configuration horaire — Sélection du mode d'affichage\n");
  RUN_TEST(test_config_index_midnight);
  RUN_TEST(test_config_index_midday);
  RUN_TEST(test_config_index_gouter);
  RUN_TEST(test_config_index_standard_am);
  RUN_TEST(test_config_index_standard_pm);

  printf("\n🎨 Couleurs — Encodage/décodage uint32 ↔ RGB\n");
  RUN_TEST(test_color_encode_pure_red);
  RUN_TEST(test_color_encode_pure_green);
  RUN_TEST(test_color_encode_pure_blue);
  RUN_TEST(test_color_encode_white);
  RUN_TEST(test_color_encode_black);
  RUN_TEST(test_color_roundtrip);
  RUN_TEST(test_color_roundtrip_all_defaults);

  printf("\n🔆 Luminosité — Incrémentation et wrapping\n");
  RUN_TEST(test_brightness_increment);
  RUN_TEST(test_brightness_wrap_around);
  RUN_TEST(test_brightness_min_value);
  RUN_TEST(test_brightness_full_cycle);

  printf("\n⏱️ Progression des minutes\n");
  RUN_TEST(test_progress_minute_0);
  RUN_TEST(test_progress_minute_30);
  RUN_TEST(test_progress_minute_59);
  RUN_TEST(test_progress_4_letters);

  printf("\n📦 Tableaux PROGMEM — Tailles et cohérence\n");
  RUN_TEST(test_progmem_sizes);
  RUN_TEST(test_progmem_indices_in_bounds);
  RUN_TEST(test_midnight_word_subset_of_phrase);

  printf(
      "\n════════════════════════════════════════════════════════════════\n");
  printf("📊 Résultats : %d/%d tests passés", tests_passed, tests_run);
  if (tests_failed > 0)
    printf(" (%d échoué%s)\n", tests_failed, tests_failed > 1 ? "s" : "");
  else
    printf(" ✅ Tous les tests passent !\n");
  printf("\n");

  return tests_failed > 0 ? 1 : 0;
}
#endif // ARDUINO
