#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "pio_matrix.pio.h" // Arquivo PIO para controle da matriz de LEDs
#include "lib/ssd1306.h"    // Biblioteca para o display OLED
#include "lib/leds.h"       // Biblioteca para os LEDs
#include "lib/buzzer.h"     // Biblioteca para o buzzer

#define MAX_USUARIOS 25       // Capacidade máxima de usuários
#define BUTTON_A 5            // BOTÃO A
#define BUTTON_B 6            // BOTÃO B
#define BUTTON_JOYSTICK 22    // BOTÃO JOYSTICK
#define DEBOUNCE_TIME 300     // tempo de debounce em ms
#define BUZZER_FREQUENCY 1000 // FREQUENCIA DO BUZZER
#define BUZZER_A 10           // PORTA DO BUZZER A
#define BUZZER_B 21           // PORTA DO BUZZER B

#define I2C_PORT i2c1 // PORTA DO i2C
#define I2C_SDA 14    // PINO DO SDA
#define I2C_SCL 15    // PINO DO SCL
#define endereco 0x3C // ENDEREÇO

#define LED_RED 13   // pino do led vermelho
#define LED_BLUE 12  // pino do led azul
#define LED_GREEN 11 // pino do led verde

SemaphoreHandle_t xSemaforoReset;    // Semáforo para reset
SemaphoreHandle_t xSemaforoUsuarios; // Semáforo para controle de usuários
SemaphoreHandle_t xMutexDisplay;     // Mutex para controle do display

ssd1306_t ssd; // Variável para o display

volatile uint usuarios_ativos = 0;              // Contador de usuários ativos
static absolute_time_t last_interrupt_time = 0; // Variável para armazenar o tempo da última interrupção

PIO pio;
uint sm;

// Tarefa: Entrada de usuário (botão A)
void vTaskAdicionaUsuario(void *pvParameters)
{

    // Quando o botão A é pressionado, incrementa o contador de usuários
    // Se o contador for menor que a capacidade máxima, atualiza o display
    // Se o contador for igual a capacidade máxima, não faz nada
    // Se o botão A não for pressionado, aguarda 100ms e verifica novamente
    while (1)
    {
        if (gpio_get(BUTTON_A) == 0)
        {

            if (xSemaphoreTake(xSemaforoUsuarios, pdMS_TO_TICKS(100)) == pdTRUE)
            {

                usuarios_ativos = usuarios_ativos + 1;
                printf("Entrou! Agora há %d usuários\n", usuarios_ativos);

                UBaseType_t uxCount;
                uxCount = uxSemaphoreGetCount(xSemaforoUsuarios); // Obtém quantos tokens estão disponíveis agora

                if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(100)) == pdTRUE)
                {
                    char buffer[20];
                    snprintf(buffer, sizeof(buffer), "Usuarios: %d", usuarios_ativos);
                    ssd1306_fill(&ssd, false);
                    ssd1306_draw_string(&ssd, buffer, 0, 0);
                    sniprintf(buffer, sizeof(buffer), "Vagas: %d", uxCount);
                    ssd1306_draw_string(&ssd, buffer, 0, 18);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xMutexDisplay);

                    // Aciona a animação de chegada do usuário
                    user_arrival_animation(pio, sm, usuarios_ativos + 1);
                }
            }
            else
            {
                printf("Capacidade máxima atingida!\n");
                // beep curto para indicar que a capacidade máxima foi atingida
                buzzer_pwm(BUZZER_A, BUZZER_FREQUENCY, 100); // Ativa o Buzzer A por 100ms
            }

            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME)); // debounce
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tarefa: Saída de usuário (botão B)
void vTaskRemoveUsuario(void *pvParameters)
{
    // Quando o botão B é pressionado, decrementa o contador de usuários
    // Se o contador for maior que 0, atualiza o display
    // Se o contador for igual a 0, não faz nada
    // Se o botão B não for pressionado, aguarda 100ms e verifica novamente
    while (1)
    {

        if (gpio_get(BUTTON_B) == 0)
        {
            if (usuarios_ativos > 0)
            {
                usuarios_ativos--;
                xSemaphoreGive(xSemaforoUsuarios);
                printf("Saiu! Agora há %d usuários\n", usuarios_ativos);
                UBaseType_t uxCount;
                uxCount = uxSemaphoreGetCount(xSemaforoUsuarios); // Obtém quantos tokens estão disponíveis agora

                if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(100)) == pdTRUE)
                {
                    char buffer[20];
                    snprintf(buffer, sizeof(buffer), "Usuarios: %d", usuarios_ativos);
                    ssd1306_fill(&ssd, false);
                    ssd1306_draw_string(&ssd, buffer, 0, 0);
                    sniprintf(buffer, sizeof(buffer), "Vagas: %d", uxCount);
                    ssd1306_draw_string(&ssd, buffer, 0, 18);
                    ssd1306_send_data(&ssd);
                    // display atualizado
                    printf("Display atualizado com %d usuários\n", usuarios_ativos);
                    xSemaphoreGive(xMutexDisplay);

                    // Aciona a animação de saída do usuário
                    user_exit_animation(pio, sm, usuarios_ativos);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void gpio_irq_handler(uint gpio, uint32_t events)
{

    absolute_time_t now = get_absolute_time();
    printf("Interrupção na GPIO %d\n", gpio);

    if (gpio == BUTTON_JOYSTICK)
    {
        // Verifica se o tempo desde a última interrupção é maior que o tempo de debounce
        // Se sim, atualiza o tempo da última interrupção e libera o semáforo de reset
        // Se não, ignora a interrupção
        {
            if (absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_TIME * 1000)
            {
                // Atualiza o tempo da última interrupção
                last_interrupt_time = now;

                BaseType_t xHigherPriorityTaskWoken = pdFALSE;                    // Incializa com pdFALSE
                xSemaphoreGiveFromISR(xSemaforoReset, &xHigherPriorityTaskWoken); // Libera o semáforo de reset
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                     // Troca o contexto da tarefa se necessário
            }
        }
    }
}

void reset_feedback(uint usuarios_ativos_antes)
{
    reset_animation(pio, sm, usuarios_ativos_antes); // Aciona a animação de reset
    // beep duplo para indicar o reset
    buzzer_pwm(BUZZER_A, BUZZER_FREQUENCY, 100); // Ativa o Buzzer A por 100ms
    vTaskDelay(pdMS_TO_TICKS(50));               // Aguarda 50ms
    buzzer_pwm(BUZZER_B, BUZZER_FREQUENCY, 100); // Ativa o Buzzer B por 100ms
    vTaskDelay(pdMS_TO_TICKS(50));
}

// Tarefa: reset da contagem via semáforo
// Reseta o contador de usuários e libera os semáforos de controle de usuários
void vTaskReset(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaforoReset, portMAX_DELAY) == pdTRUE)
            printf("Resetando contagem de usuários\n");
        uint usuarios_ativos_antes = usuarios_ativos;
        {

            for (int i = 0; i < usuarios_ativos_antes; i++)
            {
                xSemaphoreGive(xSemaforoUsuarios);
            }

            usuarios_ativos = 0;                   // Reseta o contador de usuários
            reset_feedback(usuarios_ativos_antes); // Aciona o feedback de reset
        }
    }
}

// Tarefa: Atualiza os LEDs RGB
// Atualiza os LEDs RGB de acordo com o número de usuários ativos
// Se não houver usuários ativos, acende o LED azul
// Se houver usuários ativos, acende o LED verde
// Se houver usuários ativos e o número for menor que a capacidade máxima - 1, acende o LED vermelho
// Se houver usuários ativos e o número for igual a capacidade máxima, acende o LED vermelho
// Se houver usuários ativos e o número for maior que a capacidade máxima, acende o LED vermelho
void vTaskLed(void *pvParameters)
{
    while (1)
    {
        if (usuarios_ativos == 0)
        {

            gpio_put(LED_GREEN, 0);
            gpio_put(LED_RED, 0);
            gpio_put(LED_BLUE, 1);
        }
        else if (usuarios_ativos <= MAX_USUARIOS - 2)
        {
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_RED, 0);
            gpio_put(LED_BLUE, 0);
        }
        else if (usuarios_ativos <= MAX_USUARIOS - 1)
        {
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_RED, 1);
            gpio_put(LED_BLUE, 0);
        }
        else
        {
            gpio_put(LED_GREEN, 0);
            gpio_put(LED_RED, 1);
            gpio_put(LED_BLUE, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Aguarda 100ms
    }
}

// Tarefa: Atualiza o display a cada 500ms
// Atualiza o display com o número de usuários ativos
// Se o semáforo de controle do display estiver disponível, atualiza o display
void vTaskDisplay(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UBaseType_t uxCount;
            uxCount = uxSemaphoreGetCount(xSemaforoUsuarios); // Obtém quantos tokens estão disponíveis agora
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "Usuarios: %d", usuarios_ativos);
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, buffer, 0, 0);
            sniprintf(buffer, sizeof(buffer), "Vagas: %d", uxCount);
            ssd1306_draw_string(&ssd, buffer, 0, 18);
            ssd1306_send_data(&ssd);
            xSemaphoreGive(xMutexDisplay);
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Atualiza a cada 500 ms
    }
}

// Configuração dos pinos GPIO para os botões
// Inicializa o pino, define como entrada e ativa o pull-up
void setup_gpio_button(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

// Configuração do I2C
// Inicializa o I2C, define os pinos SDA e SCL, ativa o pull-up e inicializa o display
// Configura o display e limpa a tela
void setup_i2c()
{

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Limpa o display na inicialização
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void PIO_setup()
{
    // configurações da PIO
    pio = pio0;
    uint offset = pio_add_program(pio, &pio_matrix_program);
    sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, LED_PIN);
}

void init_leds()
{
    // Inicializa os pinos do LED RGB
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);

    // Define os pinos como saída
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
}

int main()
{

    stdio_init_all();
    // Inicializa os buzzers
    initialization_buzzers(BUZZER_A, BUZZER_B);

    // Configuração dos pinos
    setup_gpio_button(BUTTON_A);
    setup_gpio_button(BUTTON_B);
    setup_gpio_button(BUTTON_JOYSTICK);
    // Configuração de interrupções
    gpio_set_irq_enabled_with_callback(
        BUTTON_JOYSTICK,
        GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_irq_handler);
    // Configuração dos LEDs
    init_leds();

    // Criação do semáforo binário para controle de usuários
    xSemaforoUsuarios = xSemaphoreCreateCounting(MAX_USUARIOS, MAX_USUARIOS); // inicia a contagem com o máximo de usuários(25)
    xSemaforoReset = xSemaphoreCreateBinary();
    xMutexDisplay = xSemaphoreCreateMutex();

    // Configuração do I2C
    setup_i2c();
    // Configuração da PIO
    PIO_setup();

    xTaskCreate(vTaskAdicionaUsuario, "Adiciona Usuario", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskRemoveUsuario, "Remove Usuario", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    xTaskCreate(vTaskDisplay, "Atualiza monitor", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    xTaskCreate(vTaskLed, "Atualiza o led", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    // Início do escalonador
    vTaskStartScheduler();

    while (1)
    {
    }
    panic_unsupported();
}
