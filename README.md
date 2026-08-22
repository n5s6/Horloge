# 🕐 HorlogeIA — Horloge à Mots Française

Une horloge à mots en français pilotée par un **ESP32-C3** et un ruban de **LEDs SK6812 RGBW**.  
L'heure est affichée sous forme de mots ("IL EST TROIS HEURES") avec des couleurs personnalisables et des animations.

---

## ✨ Fonctionnalités

| Fonctionnalité | Description |
|---|---|
| **Affichage en mots** | L'heure est affichée en français avec des phrases complètes à chaque heure pile |
| **Progression colorée** | Entre les heures, le mot de l'heure change progressivement de couleur au fil des minutes |
| **12 palettes de couleurs** | Chaque heure a sa propre paire de couleurs (base + progression), personnalisable |
| **Feu d'artifice** | Animation de feu d'artifice au démarrage et déclenchable via bouton ou interface web |
| **Anniversaires** | Feux d'artifice aléatoires à chaque heure lors de dates configurables (max 10) |
| **Interface web** | Configuration complète via un navigateur (Wi-Fi, couleurs, luminosité, fuseau horaire, anniversaires) |
| **Synchronisation NTP** | L'heure se synchronise automatiquement via Internet toutes les 24 heures |
| **Mode Point d'Accès** | Configuration initiale sans réseau Wi-Fi existant |
| **Luminosité réglable** | Ajustable par bouton physique ou via l'interface web |

### Easter Eggs 🥚

- **"MIAM"** s'affiche à la place de "MIDI" un jour sur deux
- **"GOÛTER"** s'affiche à 16h au lieu de "QUATRE HEURES"
- **"APERO"** s'affiche à 18h un jour sur deux, ainsi que tous les week-ends

---

## 🔧 Matériel Requis

| Composant | Détail |
|---|---|
| **Microcontrôleur** | ESP32-C3 (DevKit ou module équivalent) |
| **LEDs** | Ruban SK6812 RGBW — 288 LEDs (144 paires logiques) |
| **Bouton** *(optionnel)* | 1 bouton poussoir (GPIO 6, pull-up interne) |
| **Alimentation** | 5V adaptée au nombre de LEDs (~60mA max par LED × 288) |
| **Câble USB** | USB-C **avec données** (pas un câble de charge uniquement !) |

### Schéma de Câblage

```
ESP32-C3
  ├── GPIO 10  ──→  Data In du ruban SK6812
  ├── GPIO 6   ──→  Bouton poussoir (→ GND) [optionnel]
  ├── GND      ──→  GND alimentation + GND ruban
  └── 5V       ──→  5V alimentation (via le ruban, PAS via USB pour autant de LEDs)
```

> **⚠️ Important** : Ne pas alimenter 288 LEDs directement via le port USB de l'ESP32. Utilisez une alimentation 5V externe (≥ 3A recommandé) raccordée au ruban, et connectez le GND de l'alimentation au GND de l'ESP32.

---

## 📁 Structure du Projet

```
horlogeIA/
├── horlogeIA.ino       # Programme principal (setup, loop, affichage heure, tests LED)
├── config.h            # Configuration matérielle (broches, constantes, timings)
├── globals.h           # Structures, variables globales, fonctions utilitaires, données PROGMEM
├── fireworks.h         # Animation feu d'artifice (fusées + particules)
├── web_setup.h         # Serveur web (pages de configuration, gestion Wi-Fi, NTP)
├── test_horloge.cpp    # Tests unitaires (exécutables sur PC sans Arduino)
└── README.md           # Ce fichier
```

### Rôle de chaque fichier

- **`config.h`** — Toutes les constantes modifiables au même endroit (broches GPIO, nombre de LEDs, timings, limites d'animation). Modifier ce fichier pour adapter le projet à votre montage.

- **`globals.h`** — Le cœur des déclarations : structures de données, variables `extern`, fonctions utilitaires inline (`updateLEDs`, `clearLEDs`, `setPixelPair`, `xy_map`), tableaux de positions des mots en PROGMEM, et couleurs par défaut.

- **`horlogeIA.ino`** — Point d'entrée Arduino. Gère l'initialisation, la boucle principale, la lecture du bouton, le choix du mode d'affichage, et la logique d'affichage de l'heure.

- **`fireworks.h`** — Simulation de feu d'artifice avec des fusées qui montent et explosent en particules colorées. Utilise une grille virtuelle 12×12 et la fonction `xy_map()` pour le mapping.

- **`web_setup.h`** — Serveur web complet : page de configuration (Wi-Fi, fuseau horaire, luminosité, couleurs par heure), page de statut, déclenchement d'animations, connexion Wi-Fi et synchronisation NTP.

---

## 🚀 Installation Pas à Pas (Arduino IDE + ESP32-C3)

### Étape 1 — Installer Arduino IDE

1. Téléchargez **Arduino IDE 2.x** depuis [arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Installez-le normalement :
   - **macOS** : Glissez l'application dans le dossier `Applications`
   - **Windows** : Lancez l'installateur `.exe`
   - **Linux** : Extrayez l'archive `.AppImage` ou installez via Flatpak/Snap
3. Lancez Arduino IDE

> **💡** Arduino IDE 2.x est recommandé pour son interface moderne et son auto-complétion. La version 1.8.x fonctionne aussi mais l'interface diffère des captures ci-dessous.

### Étape 2 — Ajouter le support ESP32

L'ESP32-C3 n'est pas inclus par défaut. Il faut ajouter le dépôt Espressif :

1. Allez dans **Fichier** → **Préférences**
   *(sur macOS : **Arduino IDE** → **Preferences**)*
2. Trouvez le champ **« URL de gestionnaire de cartes supplémentaires »** en bas de la fenêtre
3. Collez cette URL :
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   > **💡** S'il y a déjà d'autres URL, cliquez sur l'icône à droite du champ pour ouvrir l'éditeur multi-lignes, et ajoutez l'URL sur une nouvelle ligne.
4. Cliquez sur **OK**
5. Allez dans **Outils** → **Carte** → **Gestionnaire de cartes...**
   *(ou cliquez sur l'icône 📋 dans la barre latérale gauche)*
6. Dans la barre de recherche, tapez **`esp32`**
7. Trouvez **« esp32 » par Espressif Systems**
8. Cliquez sur **Installer** (version ≥ 3.0.0 recommandée)
9. Patientez — le téléchargement fait environ 200 Mo

### Étape 3 — Installer les bibliothèques requises

Deux bibliothèques sont nécessaires. Pour chacune :

1. Allez dans **Outils** → **Gérer les bibliothèques...**
   *(ou cliquez sur l'icône 📚 dans la barre latérale gauche)*

#### 3a. FastLED
2. Recherchez **`FastLED`**
3. Trouvez **« FastLED » par Daniel Garcia**
4. Cliquez sur **Installer** (version **≥ 3.7.0** obligatoire pour le support natif RGBW)

> **⚠️** Si votre version de FastLED est < 3.7.0, le projet ne compilera pas car la fonction `.setRgbw()` n'existe pas dans les versions antérieures.

#### 3b. ESP32Time
5. Recherchez **`ESP32Time`**
6. Trouvez **« ESP32Time » par fbiego**
7. Cliquez sur **Installer**

### Étape 4 — Ouvrir le projet

1. Téléchargez ou clonez ce dépôt sur votre ordinateur
2. Dans Arduino IDE : **Fichier** → **Ouvrir...**
3. Naviguez jusqu'au dossier `horlogeIA/` et sélectionnez le fichier **`horlogeIA.ino`**
4. Arduino IDE ouvre automatiquement tous les fichiers du projet dans des onglets séparés

> **💡** Arduino exige que le fichier `.ino` soit dans un dossier portant le même nom. Le dossier doit donc s'appeler `horlogeIA/`.

### Étape 5 — Configurer la carte ESP32-C3

1. Allez dans **Outils** → **Carte** → **esp32** → **ESP32C3 Dev Module**

   > Si `ESP32C3 Dev Module` n'apparaît pas dans la liste, retournez à l'**Étape 2** et vérifiez que le package ESP32 est bien installé.

2. Toujours dans le menu **Outils**, configurez les paramètres suivants :

| Paramètre | Valeur | Explication |
|---|---|---|
| **Board** | `ESP32C3 Dev Module` | Type de microcontrôleur |
| **USB CDC On Boot** | `Enabled` | Active le moniteur série via USB natif |
| **Upload Speed** | `921600` | Vitesse de téléversement maximale stable |
| **Flash Size** | `4MB (32Mb)` | Taille standard pour la plupart des modules ESP32-C3 |
| **Partition Scheme** | `Default 4MB with spiffs` | Schéma de partitionnement standard |

### Étape 6 — Connecter l'ESP32-C3

1. Branchez l'ESP32-C3 à votre ordinateur avec un **câble USB-C de données**

   > **⚠️ Piège classique** : Certains câbles USB-C ne transportent que l'alimentation (pas de données). Si le port série n'apparaît pas, essayez un autre câble. Les câbles fournis avec les chargeurs de téléphone sont souvent en cause.

2. Allez dans **Outils** → **Port** et sélectionnez le port de votre ESP32 :

   | OS | Nom du port typique |
   |---|---|
   | **macOS** | `/dev/cu.usbmodem*` ou `/dev/cu.SLAB_USBtoUART` |
   | **Windows** | `COM3`, `COM4`, `COM5`, etc. |
   | **Linux** | `/dev/ttyUSB0` ou `/dev/ttyACM0` |

   > **💡 Le port n'apparaît pas ?**
   > - Débranchez et rebranchez le câble USB
   > - Essayez un **autre câble** USB (c'est la cause n°1)
   > - Essayez un **autre port USB** de votre ordinateur
   > - **macOS/Linux** : Vérifiez avec `ls /dev/cu.*` (Mac) ou `ls /dev/ttyUSB* /dev/ttyACM*` (Linux)
   > - **Windows** : Installez les drivers [CP210x (Silicon Labs)](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) ou [CH340 (WCH)](http://www.wch-ic.com/downloads/CH341SER_ZIP.html) selon votre module

### Étape 7 — Compiler et téléverser

#### 7a. Vérifier la compilation

1. Cliquez sur le bouton **✓ Vérifier** (en haut à gauche)
2. La compilation prend **30 à 60 secondes** la première fois (plus rapide ensuite)
3. En bas de l'écran, vérifiez :
   ```
   Sketch uses XXXXX bytes (XX%) of program storage space.
   Global variables use XXXXX bytes (XX%) of dynamic memory.
   ```
   → Si vous voyez ce message, la compilation a réussi ✅  
   → Si vous voyez des erreurs en rouge, vérifiez que les bibliothèques sont bien installées (Étape 3)

#### 7b. Téléverser le programme

1. Cliquez sur le bouton **→ Téléverser** (flèche à droite du bouton Vérifier)
2. La console affiche :
   ```
   Connecting........_____
   ```
   puis le pourcentage d'avancement :
   ```
   Writing at 0x00010000... (3%)
   Writing at 0x00014000... (6%)
   ...
   ```
3. Attendez le message final :
   ```
   Hard resetting via RTS pin...
   ```
   → **Le téléversement est terminé !** ✅

#### 🔧 En cas d'échec du téléversement

Si vous voyez `Failed to connect to ESP32-C3: No serial data received` :

**Méthode 1 — Bouton BOOT** *(la plus courante)*
1. Cliquez sur **→ Téléverser** dans Arduino IDE
2. Quand la console affiche `Connecting........`, **maintenez appuyé** le bouton **BOOT** sur l'ESP32-C3
3. **Relâchez** quand le pourcentage d'avancement commence à s'afficher

**Méthode 2 — Forcer le mode Download**
1. Maintenez le bouton **BOOT** enfoncé
2. Tout en gardant BOOT appuyé, appuyez brièvement sur **RST** (ou **EN**)
3. Relâchez **BOOT**
4. Cliquez sur **→ Téléverser** dans Arduino IDE

**Méthode 3 — Changer la configuration USB**
- Dans **Outils** → **USB CDC On Boot**, essayez de basculer entre `Enabled` et `Disabled`
- Essayez un autre port USB ou un autre câble

### Étape 8 — Vérifier le fonctionnement

1. Ouvrez le **Moniteur Série** :
   - **Outils** → **Moniteur Série** *(ou cliquez sur l'icône 🔍 en haut à droite)*
2. Réglez la vitesse sur **115200 baud** (sélecteur en bas à droite du moniteur)
3. Appuyez sur le bouton **RST** de l'ESP32-C3 pour redémarrer
4. Vous devriez voir dans le moniteur :
   ```
   BOOTING ESP32 WORD CLOCK
   Aucune configuration Wi-Fi. Démarrage du mode Point d'Accès.
   Configuring access point...
   AP IP address: 192.168.4.1
   HTTP server started. Open http://192.168.4.1 in your browser.
   ```

### Étape 9 — Configuration initiale (Wi-Fi et heure)

1. Sur votre **téléphone** ou **ordinateur**, ouvrez les paramètres Wi-Fi
2. Connectez-vous au réseau **"Clock-Setup"** (pas de mot de passe)
3. Ouvrez un navigateur et allez sur **`http://192.168.4.1`**
4. La page de configuration s'affiche. Remplissez :

   | Champ | Valeur | Exemple |
   |---|---|---|
   | **SSID** | Nom de votre Wi-Fi domestique | `MaBox-WiFi` |
   | **Mot de passe** | Mot de passe du Wi-Fi | `•••••••` |
   | **Décalage UTC** | Votre fuseau horaire | `1` (France hiver) ou `2` (France été) |
   | **Heure d'été** | Cocher si applicable | ☑️ (entre mars et octobre en France) |
   | **Luminosité** | Ajuster au goût | `50%` |
   | **Couleurs** | Personnaliser par heure | Sélecteurs de couleur visuels |

5. Cliquez sur **"Enregistrer & Redémarrer"**
6. L'horloge redémarre, se connecte à votre Wi-Fi et synchronise l'heure via NTP
7. Un **feu d'artifice de bienvenue** s'affiche, puis l'horloge commence !

> **💡** Une fois connectée à votre Wi-Fi, l'interface de configuration reste accessible via l'adresse IP locale de l'horloge (affichée dans le moniteur série au démarrage). Vous pouvez modifier les couleurs et la luminosité à tout moment.

---

## 🎮 Utilisation

### Bouton Physique *(optionnel)*

Le bouton est facultatif : toutes ces actions sont également disponibles via l'interface web.

| Action | Durée d'appui | Effet |
|---|---|---|
| Appui court | < 3 secondes | Entre en mode réglage luminosité (appuis successifs pour ajuster) |
| Appui long | ≥ 3 secondes | Lance l'animation feu d'artifice (30 secondes) |
| Appui pendant une animation | Quelconque | Arrête l'animation et revient au mode horloge |

### Interface Web

Accessible via l'adresse IP de l'horloge sur votre réseau local (affichée dans le moniteur série au démarrage).

- **Page principale** (`/`) : Configuration Wi-Fi, fuseau horaire, luminosité, couleurs par heure, et dates d'anniversaires (JJ/MM)
- **Page statut** (`/status`) : Heure actuelle, LEDs allumées, mode actif
- **Animations** : Feu d'artifice, chenillard, test pixel par pixel

### 🎉 Feux d'artifice d'Anniversaire

Via l'interface web, vous pouvez configurer jusqu'à 10 dates d'anniversaires au format `JJ/MM` (le 1er janvier et le 24 décembre sont inclus par défaut). 
Lors d'un jour d'anniversaire :
- Un feu d'artifice se déclenche automatiquement **une fois par heure**.
- La minute de déclenchement (entre 0 et 59) est **tirée au sort** au début de chaque heure, rendant l'événement totalement imprévisible et surprenant !

---

## 🎨 Personnalisation des Couleurs

Chaque heure (0h à 11h, réutilisées pour 12h–23h) possède deux couleurs :

1. **Couleur de base** : Couleur du mot/phrase à la minute 0
2. **Couleur de progression** : Les lettres passent progressivement de la couleur de base à cette couleur au fil des minutes

Les couleurs sont modifiables :
- Via l'**interface web** (sélecteurs de couleur visuels)
- En modifiant le tableau `defaultColors_PGM` dans `globals.h` (valeurs par défaut)

---

## 🔌 Détails Techniques

### LEDs SK6812 RGBW

Le projet utilise le support **natif RGBW de FastLED** (≥ 3.7.0) via :
```cpp
FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());
```
Cela élimine le besoin de tout "hack RGBW" externe et garantit un affichage sans artefacts.

### Organisation du Ruban

- **288 LEDs physiques** organisées en **144 paires logiques**
- Chaque paire (2 LEDs côte à côte) forme un "pixel" de l'horloge
- Le ruban est disposé en **serpentin** (zigzag) sur une grille 12×12
- La fonction `xy_map()` gère la conversion coordonnées → index

### Stockage Persistant

Les paramètres sont sauvegardés en **NVS** (Non-Volatile Storage) de l'ESP32 :
- SSID et mot de passe Wi-Fi
- Fuseau horaire et heure d'été
- Luminosité
- 12 paires de couleurs personnalisées

---

## 🧪 Tests Unitaires

Le fichier `test_horloge.cpp` contient des tests unitaires qui vérifient la logique du projet **sans matériel Arduino**. Les fonctions pures (mapping, couleurs, luminosité, etc.) sont testées en simulant les types Arduino/FastLED avec des mocks légers.

### Prérequis

Un compilateur C++17 suffit. Aucune bibliothèque externe n'est nécessaire.

| OS | Compilateur | Installation |
|---|---|---|
| **macOS** | `clang++` (inclus avec Xcode) | `xcode-select --install` |
| **Linux (Debian/Ubuntu)** | `g++` | `sudo apt install g++` |
| **Linux (Fedora)** | `g++` | `sudo dnf install gcc-c++` |
| **Linux (Arch)** | `g++` | `sudo pacman -S gcc` |

### Lancer les tests

```bash
# Se placer dans le dossier du projet
cd horlogeIA/

# macOS (clang++)
clang++ -std=c++17 -Wall -Wextra -o test_horloge test_horloge.cpp && ./test_horloge

# Linux (g++)
g++ -std=c++17 -Wall -Wextra -o test_horloge test_horloge.cpp && ./test_horloge
```

En cas de succès, la sortie affiche :
```
🧪 HorlogeIA — Tests Unitaires
════════════════════════════════════════════════════════════════

📐 xy_map() — Mapping grille → index LED
  test_xy_map_origin                                     ✅ PASS
  ...

════════════════════════════════════════════════════════════════
📊 Résultats : 35/35 tests passés ✅ Tous les tests passent !
```

Le programme retourne le code de sortie **0** si tous les tests passent, **1** sinon (compatible avec les pipelines CI).

### Ce qui est testé

| Suite | Nb | Description |
|---|---|---|
| `xy_map()` | 7 | Mapping serpentin (lignes paires/impaires), hors limites, unicité de chaque index |
| `setPixelPair()` | 5 | Écriture par paire, dernière paire valide, hors limites, écrasement |
| Config horaire | 5 | Cas spéciaux (Minuit→0, Midi→0, Goûter→4) et cas général (AM/PM) |
| Couleurs | 7 | Encodage/décodage `uint32` ↔ `CRGB`, aller-retour sur les couleurs par défaut |
| Luminosité | 4 | Incrémentation par paliers de 20, wrapping au-delà de 250, cycle complet |
| Progression | 4 | Ratio de progression colorée à minute 0, 30, 59 et pour des mots courts |
| PROGMEM | 3 | Tailles des tableaux, indices dans les bornes, inclusion (mot ⊂ phrase) |

### Ajouter un test

1. Écrire une fonction `TEST(nom_du_test) { ... }` dans `test_horloge.cpp`
2. Utiliser les macros `ASSERT_EQ(valeur, attendu)` et `ASSERT_TRUE(condition)`
3. Ajouter `RUN_TEST(nom_du_test);` dans la fonction `main()`

### Nettoyage

Après exécution, le binaire `test_horloge` peut être supprimé :
```bash
rm test_horloge
```

Il est déjà ignoré par défaut si vous avez un `.gitignore` qui exclut les binaires.

---

## 🛠️ Astuce Technique : Le "Hack RGBW" pour FastLED

Les rubans SK6812 possèdent 4 canaux (Rouge, Vert, Bleu, **Blanc**). Par défaut, la bibliothèque **FastLED** est fortement optimisée pour des rubans à 3 canaux (RGB). 
Bien que les versions récentes de FastLED intègrent un support RGBW expérimental (via `setRgbw()`), son utilisation pose très fréquemment des problèmes sur ESP32 (décalage d'un octet, couleurs erronées).

Pour piloter parfaitement les SK6812 de cette horloge, le projet implémente le très répandu **"Memory Punning Hack"** :

1. **Structure sur-mesure** : Une structure `CRGBW` est déclarée dans `globals.h` pour reproduire l'ordre mémoire physique exact des puces SK6812 (G, R, B, W) sur 4 octets.
2. **Tromperie mémoire** : On déclare le tableau de LEDs `leds` avec notre type `CRGBW`, mais on indique à FastLED qu'il s'agit d'un tableau `CRGB` standard comportant virtuellement 33% de pixels supplémentaires (`(NUM_LEDS * 4) / 3`).
3. **Protocole brut** : En initialisant l'affichage avec le driver classique `WS2812` et l'ordre de couleurs `RGB`, FastLED se contente de transmettre la mémoire telle quelle, sans chercher à la remanier.
4. **Résultat** : La puce physique SK6812 lit la trame de 4 octets à la perfection. Le canal blanc fonctionne, les couleurs sont justes, et les effets natifs de FastLED (comme `fadeToBlackBy`) fonctionnent naturellement sur toute la mémoire.

---

## 📝 Licence

Projet personnel — Libre d'utilisation et de modification.
