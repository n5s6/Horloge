#ifndef FIREWORKS_H
#define FIREWORKS_H

// =====================================================================================
//  FIREWORKS.H — Animation de feu d'artifice
//
//  Simule un feu d'artifice sur la grille 12×12 de l'horloge :
//    1. Des fusées (Rocket) montent depuis le bas de la grille
//    2. En atteignant leur altitude cible, elles explosent
//    3. L'explosion génère des particules (Particle) colorées qui retombent
//    4. Les particules s'estompent progressivement avant de disparaître
//
//  Utilise la grille virtuelle via xy_map() pour convertir les coordonnées
//  (x, y) en indices de paires logiques sur le ruban LED.
// =====================================================================================

#include "globals.h"

/**
 * @brief Lance une nouvelle fusée depuis le bas de la grille.
 *
 * Cherche un emplacement libre dans le tableau rockets[] et initialise
 * une nouvelle fusée avec une position, une altitude cible et une couleur aléatoires.
 * Ne lance qu'une seule fusée par appel.
 */
void launchRocket()
{
    for (int i = 0; i < MAX_ROCKETS; ++i)
    {
        if (!rockets[i].active)
        {
            rockets[i].active = true;
            rockets[i].x = random(GRID_WIDTH / 4, GRID_WIDTH * 3 / 4);      // Colonne aléatoire (zone centrale)
            rockets[i].y = GRID_HEIGHT - 1;                                  // Part du bas de la grille
            rockets[i].target_y = random(GRID_HEIGHT / 4, GRID_HEIGHT / 2);  // Explose dans le tiers supérieur
            rockets[i].color = CHSV(random8(), 255, 255);                    // Couleur HSV aléatoire saturée
            rockets[i].tail_length = 3;                                      // Longueur de la traînée
            return;
        }
    }
}

/**
 * @brief Génère une explosion de particules à une position donnée.
 *
 * Crée entre 15 et MAX_PARTICLES_PER_EXPLOSION particules qui partent dans
 * des directions aléatoires avec des vitesses variées. Les couleurs sont
 * des variations de la teinte de la fusée d'origine.
 *
 * @param x  Position X de l'explosion (sur la grille)
 * @param y  Position Y de l'explosion (sur la grille)
 * @param explosion_base_color  Couleur HSV de la fusée (base pour les variations)
 */
void explode(float x, float y, CHSV explosion_base_color)
{
    int particles_to_launch = random(15, MAX_PARTICLES_PER_EXPLOSION + 1);
    for (int i = 0; i < particles_to_launch; ++i)
    {
        if (activeParticleCount < MAX_TOTAL_PARTICLES)
        {
            // Chercher un emplacement libre dans le tableau de particules
            for (int p_idx = 0; p_idx < MAX_TOTAL_PARTICLES; ++p_idx)
            {
                if (!particles[p_idx].active)
                {
                    particles[p_idx].active = true;
                    particles[p_idx].x = x;
                    particles[p_idx].y = y;

                    // Direction et vitesse aléatoires (360°)
                    float angle = random(0, 360) * DEG_TO_RAD;
                    float speed = random(50, 150) / 100.0;
                    particles[p_idx].vx = cos(angle) * speed;
                    particles[p_idx].vy = sin(angle) * speed;

                    // Couleur : variation de teinte autour de la couleur de base
                    CHSV current_particle_hsv = explosion_base_color;
                    current_particle_hsv.hue += random8(-25, 26);   // ±25 de variation de teinte
                    current_particle_hsv.sat = 255;                  // Saturation maximale
                    current_particle_hsv.val = random8(180, 255);    // Luminosité variable

                    hsv2rgb_rainbow(current_particle_hsv, particles[p_idx].color);
                    particles[p_idx].color.fadeLightBy(random8(100)); // Légère variation de luminosité

                    particles[p_idx].lifespan = random(30, 60);      // 30 à 60 frames de vie
                    activeParticleCount++;
                    break;
                }
            }
        }
        else
        {
            break;  // Plus d'emplacement disponible
        }
    }
}

/**
 * @brief Calcule et affiche une frame de l'animation feu d'artifice.
 *
 * Appelée en boucle depuis loop() tant que fireworksMode est actif.
 * Gère trois phases simultanément :
 *   1. Lancement périodique de nouvelles fusées (toutes les 700ms)
 *   2. Montée des fusées avec traînée lumineuse
 *   3. Mouvement des particules avec gravité et estompage
 */
void runFireworksFrame()
{
    // Estompage progressif du buffer → effet de traînée sur toutes les LEDs
    fadeToBlackBy((CRGB*)&leds[0], (NUM_LEDS * 4) / 3, 40);

    // Lancer une nouvelle fusée toutes les 700ms
    EVERY_N_MILLISECONDS(700) {
        if (fireworksMode) launchRocket();
    }

    // --- Mise à jour des fusées ---
    for (int i = 0; i < MAX_ROCKETS; ++i)
    {
        if (rockets[i].active)
        {
            CRGB rocket_display_color;
            hsv2rgb_rainbow(rockets[i].color, rocket_display_color);

            // Dessiner la traînée de la fusée (du plus sombre au plus lumineux)
            for (int t = 1; t <= rockets[i].tail_length; ++t)
            {
                int tail_pixel_idx = xy_map(round(rockets[i].x), round(rockets[i].y + t));
                if (tail_pixel_idx != -1)
                {
                    CRGB tail_color = rocket_display_color;
                    tail_color.fadeToBlackBy(t * (255 / (rockets[i].tail_length + 1)));
                    setPixelPair(tail_pixel_idx, tail_color);
                }
            }

            // Dessiner la tête de la fusée
            int rocket_pixel_idx = xy_map(round(rockets[i].x), round(rockets[i].y));
            if (rocket_pixel_idx != -1) {
                setPixelPair(rocket_pixel_idx, rocket_display_color);
            }

            // Faire monter la fusée
            rockets[i].y -= 0.3;

            // Vérifier si la fusée a atteint sa cible → explosion
            if (rockets[i].y <= rockets[i].target_y) {
                explode(rockets[i].x, rockets[i].y, rockets[i].color);
                rockets[i].active = false;
            }
            // Sécurité : désactiver si sortie de la grille
            if (rockets[i].y < 0) {
                rockets[i].active = false;
            }
        }
    }

    // --- Mise à jour des particules ---
    for (int i = 0; i < MAX_TOTAL_PARTICLES; ++i)
    {
        if (particles[i].active)
        {
            // Appliquer le mouvement et la gravité
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.03;  // Gravité (les particules retombent)

            particles[i].lifespan--;

            // Désactiver si durée de vie écoulée ou sortie de la grille
            if (particles[i].lifespan <= 0 ||
                particles[i].y >= GRID_HEIGHT || particles[i].y < 0 ||
                particles[i].x < 0 || particles[i].x >= GRID_WIDTH)
            {
                particles[i].active = false;
                activeParticleCount--;
            }
            else
            {
                // Dessiner la particule avec estompage en fin de vie
                int particle_pixel_idx = xy_map(round(particles[i].x), round(particles[i].y));
                if (particle_pixel_idx != -1)
                {
                    CRGB particle_draw_color = particles[i].color;
                    if (particles[i].lifespan < 30) {
                        // Estompage progressif sur la dernière moitié de la vie
                        particle_draw_color.nscale8_video(map(particles[i].lifespan, 0, 29, 30, 255));
                    }
                    setPixelPair(particle_pixel_idx, particle_draw_color);
                }
            }
        }
    }
}

#endif // FIREWORKS_H
