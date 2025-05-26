# 🧠 Painel de Controle Interativo com Acesso Concorrente

Este projeto implementa um sistema de controle de acesso usando a **Raspberry Pi Pico W**, aplicando conceitos de multitarefa com **FreeRTOS**, semáforos e mutex. O sistema simula o controle do fluxo de usuários em um ambiente, oferecendo feedback visual e sonoro em tempo real.

---

## 🎯 Objetivo

Desenvolver um painel interativo multitarefa capaz de:
- Adicionar e remover usuários conforme a capacidade do sistema.
- Exibir a quantidade de usuários ativos e vagas disponíveis.
- Alertar quando o sistema estiver cheio.
- Reiniciar a contagem via interrupção.
- Utilizar **semáforos de contagem**, **binários** e **mutexes** de forma segura e concorrente.

---

## ⚙️ Funcionalidades

- **Botão A**: Adiciona um usuário ao sistema.
- **Botão B**: Remove um usuário do sistema.
- **Joystick (Botão Reset)**: Reinicia o sistema.
- **Display OLED**: Exibe número de usuários e vagas disponíveis.
- **LED RGB**: Indica status de ocupação:
  - Azul: Nenhum usuário
  - Verde: Espaço disponível
  - Amarelo: Última vaga
  - Vermelho: Capacidade máxima atingida
- **Buzzer**: Feedback sonoro:
  - Beep curto: Sistema cheio
  - Dois beeps: Reset executado
- **Matriz de LEDs (via PIO)**: Animações de entrada/saída de usuários.

---

## 🧵 Organização do Código

- `vTaskAdicionaUsuario`: Task responsável por adicionar usuários com controle por semáforo de contagem.
- `vTaskRemoveUsuario`: Task que libera uma vaga no sistema.
- `vTaskReset`: Task que reinicia a contagem via semáforo binário acionado por interrupção.
- `vTaskDisplay`: Atualiza o display com número de usuários e vagas.
- `vTaskLed`: Atualiza o LED RGB conforme o estado do sistema.

---

## 🛠️ Tecnologias Utilizadas

- **FreeRTOS** para gerenciamento de tarefas e sincronização.
- **Semáforos e Mutexes**:
  - `xSemaphoreCreateCounting`: controla ocupação do sistema.
  - `xSemaphoreCreateBinary`: dispara o reset via ISR.
  - `xSemaphoreCreateMutex`: protege o display contra acessos simultâneos.
- **SSD1306** para display OLED via I2C.
- **PIO Assembly** para controle da matriz de LEDs.
- **GPIOs** para botões e LEDs.

---

## 📦 Hardware Necessário

- Raspberry Pi Pico W
- Display OLED (SSD1306, I2C)
- Buzzer (x2)
- LED RGB
- Matriz de LEDs
- Botões: A, B e joystick
- Resistores de pull-up (caso não use internos)

---
## 📸 Demonstração

