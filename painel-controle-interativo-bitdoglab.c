#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MAX_USUARIOS 25
#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_JOYSTICK 28

SemaphoreHandle_t xSemaforoReset;
SemaphoreHandle_t xSemaforoUsuarios;
SemaphoreHandle_t xMutexDisplay;

int usuarios_ativos = 0;

// Tarefa: Entrada de usuário (botão A)
void vTaskAdicionaUsuario(void *pvParameters)
{
    while (1)
    {
        if (gpio_get(BUTTON_A) == 0)
        {
            if (xSemaphoreTake(xSemaforoUsuarios, 0) == pdTRUE)
            {
                usuarios_ativos++;
                printf("Entrou! Agora há %d usuários\n", usuarios_ativos);

                if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(100)) == pdTRUE)
                {
                    // display atualizado
                    printf("Display atualizado com %d usuários\n", usuarios_ativos);
                    xSemaphoreGive(xMutexDisplay);
                }
            }
            else
            {
                printf("Capacidade máxima atingida!\n");
            }
            vTaskDelay(pdMS_TO_TICKS(300)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Tarefa: Saída de usuário (botão B)
void vTaskRemoveUsuario(void *pvParameters)
{
    while (1)
    {
        if (gpio_get(BUTTON_B) == 0)
        {
            if (usuarios_ativos > 0)
            {
                usuarios_ativos--;
                xSemaphoreGive(xSemaforoUsuarios);
                printf("Saiu! Agora há %d usuários\n", usuarios_ativos);

                if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(100)) == pdTRUE)
                {
                    // display atualizado
                    printf("Display atualizado com %d usuários\n", usuarios_ativos);
                    xSemaphoreGive(xMutexDisplay);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(300)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void gpio_callback(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;                    // Incializa com pdFALSE
    xSemaphoreGiveFromISR(xSemaforoReset, &xHigherPriorityTaskWoken); // Libera o semáforo de reset
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                     // Troca o contexto da tarefa se necessário
}

void gpio_irq_handler(uint gpio, uint32_t events)
{
    if (gpio == BUTTON_JOYSTICK)
    {
        gpio_callback(gpio, events);
    }
}

// Tarefa: reset da contagem via semáforo
void vTaskReset(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaforoReset, portMAX_DELAY) == pdTRUE)
        {
            usuarios_ativos = 0;

            for (int i = 0; i < MAX_USUARIOS; i++)
            {
                xSemaphoreGive(xSemaforoUsuarios);
            }
        }
    }
}

void setup_gpio_button(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

int main()
{
    stdio_init_all();

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
    // Criação do semáforo binário para controle de usuários
    xSemaforoUsuarios = xSemaphoreCreateCounting(MAX_USUARIOS, MAX_USUARIOS);
    xSemaforoReset = xSemaphoreCreateBinary();
    xMutexDisplay = xSemaphoreCreateMutex();

    xTaskCreate(vTaskAdicionaUsuario, "Adiciona Usuario", 256, NULL, 1, NULL);
    xTaskCreate(vTaskRemoveUsuario, "Remove Usuario", 256, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", 256, NULL, 2, NULL);

    // Início do escalonador
    vTaskStartScheduler();

    while (1)
    {
        // nunca chega aqui
    }
    panic_unsupported();
}
