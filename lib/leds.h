#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>
#include "hardware/pio.h"
#include "pico/stdlib.h"

#define PIXELS 25
#define LED_PIN 7

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    float intensity;
} pixel;

typedef enum
{
    BLACK,
    BROWN,
    RED,
    ORANGE,
    YELLOW,
    GREEN,
    BLUE,
    VIOLET,
    GRAY,
    WHITE,
    NUM_COLORS
} color_options;

static const char *color_names[NUM_COLORS] = {
    "Preto",
    "Marrom",
    "Vermelho",
    "Laranja",
    "Amarelo",
    "Verde",
    "Azul",
    "Violeta",
    "Cinza",
    "Branco"};

typedef pixel frame[PIXELS];

// Função para definir a intensidade das cores
uint32_t matrix_rgb(uint r, uint g, uint b, float intensity);

// Função para acionar a matriz de LEDs WS2812B
void draw_pio(pixel *draw, PIO pio, uint sm);

void test_matrix(PIO pio, uint sm);

// animacao de chegada do usuario
void user_arrival_animation(PIO pio, uint sm, uint user_count, color_options color);
// animacao de saida do usuario
void user_exit_animation(PIO pio, uint sm, uint user_count, color_options color);
// animacao de reset
void reset_animation(PIO pio, uint sm, uint user_count);

#endif