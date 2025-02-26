#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "ws2812.pio.h"

// Definições dos pinos
#define BTN_A_PIN 5
#define BTN_B_PIN 6
#define BUZZER_PIN 10
#define JOYSTICK_BTN_PIN 22
#define JOYSTICK_Y_PIN 26
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12
#define WS2812_PIN 7
#define NUM_PIXELS 25

// Configurações do sistema
const char *ADMIN_PASSWORD = "admin123";
char machine_a_password[20] = "1234";
char machine_b_password[20] = "5678";
bool system_locked = false;
int failed_attempts = 0;

// Variáveis globais
volatile bool btn_a_pressed = false;
volatile bool btn_b_pressed = false;
volatile uint64_t last_a_time = 0;
volatile uint64_t last_b_time = 0;
PIO pio = pio0;
int sm = 0;

// Protótipos das funções
void setup_adc();
void setup_buttons();
void buzzer_init();
void play_sound(int frequency, int duration);
void rgb_led_init();
void set_rgb_led(bool red, bool green, bool blue);
bool authenticate_user(const char *correct_password);
bool perform_safety_steps_machine_A();
bool perform_safety_steps_machine_B();
void activate_machine(const char *machine);
void handle_failed_attempt();
void admin_mode();
void display_menu();
void show_animation(uint8_t r, uint8_t g, uint8_t b);
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(uint32_t pixel_grb);

// Efeitos de senha
void effect_password_error();
void effect_password_success();

// Funções para animação na matriz de LEDs
void display_matriz(bool *buffer, uint8_t r, uint8_t g, uint8_t b);
void play_error_pass_animation();
void play_success_pass_animation();

// Animações para a matriz de LEDs (baseadas no código fornecido)
bool x_animation[3][NUM_PIXELS] = {
    {
        1, 0, 0, 0, 1,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        1, 0, 0, 0, 1
    },
    {
        1, 0, 0, 0, 1,
        0, 1, 0, 1, 0,
        0, 0, 0, 0, 0,
        0, 1, 0, 1, 0,
        1, 0, 0, 0, 1
    },
    {
        1, 0, 0, 0, 1,
        0, 1, 0, 1, 0,
        0, 0, 1, 0, 0,
        0, 1, 0, 1, 0,
        1, 0, 0, 0, 1
    }
};

bool ok_animation[3][NUM_PIXELS] = {
    {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0
    },
    {
        0, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 0, 0
    },
    {
        1, 1, 1, 1, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 1, 1, 1, 1
    }
};

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

void display_matriz(bool *buffer, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(buffer[i] ? color : 0);
    }
}

// Animaçõees para erro e acerto de senha
void play_error_pass_animation() {
    int num_frames = 3;
    for (int frame = 0; frame < num_frames; frame++) {
        display_matriz(x_animation[frame], 10, 0, 0); // Cor vermelha (intensidade 10)
        sleep_ms(150);
    }
    // Mantém o último frame por 1 segundo
    display_matriz(x_animation[num_frames - 1], 10, 0, 0);
    sleep_ms(1000);
    bool clear[NUM_PIXELS] = {0};
    display_matriz(clear, 0, 0, 0);
}

void play_success_pass_animation() {
    int num_frames = 3;
    for (int frame = 0; frame < num_frames; frame++) {
        display_matriz(ok_animation[frame], 0, 10, 0); // Cor verde (intensidade 10)
        sleep_ms(150);
    }
    display_matriz(ok_animation[num_frames - 1], 0, 10, 0);
    sleep_ms(1000);
    bool clear[NUM_PIXELS] = {0};
    display_matriz(clear, 0, 0, 0);
}

// LEITURA VIA SERIAL
void read_line(char *buffer, int max_length) {
    int count = 0;
    while (true) {
        int c = getchar_timeout_us(100000); // 100ms
        if (c == PICO_ERROR_TIMEOUT) {
            continue;
        }
        if (c == '\n' || c == '\r') {
            buffer[count] = '\0';
            printf("\r\n");
            return;
        }
        if (count < max_length - 1) {
            buffer[count++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

int read_int() {
    char buffer[20];
    read_line(buffer, sizeof(buffer));
    return atoi(buffer);
}

// Função de callback para interrupções dos botões
void gpio_callback(uint gpio, uint32_t events) {
    uint64_t current_time = time_us_64();
    if (gpio == BTN_A_PIN) {
        if (current_time - last_a_time > 300000) { // Debounce 300ms
            btn_a_pressed = true;
            last_a_time = current_time;
        }
    } else if (gpio == BTN_B_PIN) {
        if (current_time - last_b_time > 300000) {
            btn_b_pressed = true;
            last_b_time = current_time;
        }
    }
}

// Configuração do ADC para o joystick
void setup_adc() {
    adc_init();
    adc_gpio_init(JOYSTICK_Y_PIN);
}

uint16_t read_joystick_y() {
    adc_select_input(0);
    return adc_read();
}

// Configuração dos botões
void setup_buttons() {
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    
    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    
    gpio_init(JOYSTICK_BTN_PIN);
    gpio_set_dir(JOYSTICK_BTN_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_BTN_PIN);
    
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true);
}

// Configuração do buzzer (usando PWM)
void buzzer_init() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Função para tocar som via PWM
void play_sound(int frequency, int duration) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    
    if (frequency == 0) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        sleep_ms(duration);
        return;
    }
    
    float divider = 20.0;
    pwm_set_clkdiv(slice_num, divider);
   
    uint16_t wrap = (125000000 / (frequency * divider)) - 1;
    pwm_set_wrap(slice_num, wrap);
    
    // Configura o duty cycle em 50%
    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
    sleep_ms(duration);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Configuração dos LEDs RGB
void rgb_led_init() {
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
}

void set_rgb_led(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

// Efeitos para senha incorreta e correta (incluindo animações)
void effect_password_error() {
    set_rgb_led(true, false, false);
    play_sound(200, 1000);
    set_rgb_led(false, false, false);
    play_error_pass_animation();
}

void effect_password_success() {
    set_rgb_led(false, true, false);
    play_sound(1200, 1000);
    sleep_ms(200);
    set_rgb_led(false, false, false);
    play_success_pass_animation();
}

// Animação para a matriz de LEDs de Ativação ou não Ativação da Máquina
void show_animation(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    uint32_t strip[NUM_PIXELS];
    
    // Inicializa todos os LEDs apagados
    for (int i = 0; i < NUM_PIXELS; i++) {
        strip[i] = 0;
    }
    
    // Preenchimento sequencial: acende cada LED um a um
    for (int i = 0; i < NUM_PIXELS; i++) {
        strip[i] = color;
        for (int j = 0; j < NUM_PIXELS; j++) {
            put_pixel(strip[j]);
        }
        sleep_ms(50);
    }
    sleep_ms(200);
    
    // Pulso: pisca todos os LEDs juntos duas vezes
    for (int j = 0; j < 2; j++) {
        // Liga todos os LEDs
        for (int i = 0; i < NUM_PIXELS; i++) {
            strip[i] = color;
        }
        for (int i = 0; i < NUM_PIXELS; i++) {
            put_pixel(strip[i]);
        }
        sleep_ms(200);
        // Desliga todos os LEDs
        for (int i = 0; i < NUM_PIXELS; i++) {
            strip[i] = 0;
        }
        for (int i = 0; i < NUM_PIXELS; i++) {
            put_pixel(strip[i]);
        }
        sleep_ms(200);
    }
    
    // Desligamento sequencial reverso: apaga os LEDs um a um
    for (int i = 0; i < NUM_PIXELS; i++) {
        strip[i] = color;
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(strip[i]);
    }
    sleep_ms(200);
    
    for (int i = NUM_PIXELS - 1; i >= 0; i--) {
        strip[i] = 0;
        for (int j = 0; j < NUM_PIXELS; j++) {
            put_pixel(strip[j]);
        }
        sleep_ms(50);
    }
    sleep_ms(500);
    
    // Limpa a matriz
    for (int i = 0; i < NUM_PIXELS; i++) {
        strip[i] = 0;
    }
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(strip[i]);
    }
}

// Autenticação do usuário
bool authenticate_user(const char *correct_password) {
    printf("Digite a senha: ");
    char input[20];
    read_line(input, sizeof(input));
    return strcmp(input, correct_password) == 0;
}

// Procedimentos de segurança
bool perform_safety_steps_machine_A() {
    btn_a_pressed = false;

    printf("\n--- Procedimentos de Segurança - Máquina A ---\n");

    printf("PASSO 1 -> Pressione o Botao A 5 vezes...\n");
    int press_count = 0;
    uint64_t start_time = time_us_64();
    while (press_count < 5 && (time_us_64() - start_time) < 30000000) {
        if (btn_a_pressed) {
            press_count++;
            btn_a_pressed = false;
            printf("Pressionado %d/5\n", press_count);
            play_sound(1000, 100);
            set_rgb_led(0, 1, 0);
            sleep_ms(200);
            set_rgb_led(0, 0, 0);
        }
        sleep_ms(10);
    }
    if (press_count < 5) {
        printf("Falha no passo 1!\n");
        return false;
    }

    printf("PASSO 2 -> Mova o joystick para Y maximo...\n");
    start_time = time_us_64();
    bool y_max = false;
    while ((time_us_64() - start_time) < 10000000) {
        if (read_joystick_y() >= 4070) {
            y_max = true;
            break;
        }
        sleep_ms(10);
    }
    if (!y_max) {
        printf("Falha no passo 2!\n");
        return false;
    }
    play_sound(1500, 500);
    set_rgb_led(0, 1, 0);
    sleep_ms(500);
    set_rgb_led(0, 0, 0);

    printf("PASSO 3 -> Segure o Botao B por 5 segundos...\n");
    start_time = time_us_64();
    bool pressed = false;
    while ((time_us_64() - start_time) < 7000000) {
        if (!gpio_get(BTN_B_PIN)) { // Se o botão B está pressionado (nível baixo)
            uint64_t press_start = time_us_64();
            // Enquanto o botão estiver pressionado, pisca o LED azul
            while (!gpio_get(BTN_B_PIN)) {
                if (time_us_64() - press_start >= 5000000) {
                    pressed = true;
                    break;
                }
                // Liga o LED azul
                set_rgb_led(false, false, true);
                sleep_ms(100);
                // Desliga o LED
                set_rgb_led(false, false, false);
                sleep_ms(100);
            }
            if (pressed) break;
        }
        sleep_ms(10);
    }
    if (!pressed) {
        printf("Falha no passo 3!\n");
        return false;
    }
    play_sound(2000, 1000);
    set_rgb_led(false, true, false);  // Indica sucesso com LED verde
    sleep_ms(1000);
    set_rgb_led(false, false, false);

    return true;
}

bool perform_safety_steps_machine_B() {

    btn_a_pressed = false;

    printf("\n--- Procedimentos de Segurança - Máquina B ---\n");

    printf("PASSO 1 -> Aperte o Botao A 3 vezes para transformar o LED de vermelho para verde.\n");
    set_rgb_led(true, false, false); // LED vermelho
    int press_count = 0;
    uint64_t start_time = time_us_64();
    while (press_count < 3 && (time_us_64() - start_time) < 30000000) { // Timeout de 30 segundos
        if (btn_a_pressed) {
            press_count++;
            btn_a_pressed = false;
            printf("Pressionado %d/3\n", press_count);
            play_sound(1000, 100);
            sleep_ms(200);
        }
        sleep_ms(10);
    }
    if (press_count < 3) {
        printf("Falha no passo 1!\n");
        return false;
    }
    // Indica sucesso no Passo 1: LED fica verde brevemente
    set_rgb_led(false, true, false);
    sleep_ms(500);
    set_rgb_led(false, false, false);

    // Passo 2: Pressione o botão do joystick e mova-o para o valor mínimo (menor que 20)
    printf("PASSO 2 -> Pressione o botão do joystick e mova-o eixo Y para o valor mínimo.\n");
    start_time = time_us_64();
    bool joystick_valid = false;
    while ((time_us_64() - start_time) < 10000000) { // Timeout de 10 segundos
        // Verifica se o botão do joystick está pressionado (nível baixo)
        if (!gpio_get(JOYSTICK_BTN_PIN)) {
            uint16_t val = read_joystick_y();
            if (val < 20) {
                joystick_valid = true;
                break;
            }
        }
        sleep_ms(10);
    }
    if (!joystick_valid) {
        printf("Falha no passo 2!\n");
        return false;
    }
    play_sound(1500, 500);
    set_rgb_led(false, true, false);
    sleep_ms(500);
    set_rgb_led(false, false, false);

    // Passo 3: Pressione os botões A e B simultaneamente por 3 segundos, com LED azul piscando durante o período
    printf("Passo 3 ->. Pressione os botões A e B simultaneamente por 3 segundos.\n");
    start_time = time_us_64();
    bool pressed = false;
    while ((time_us_64() - start_time) < 5000000) { // Timeout de 5 segundos para iniciar o procedimento
        if (!gpio_get(BTN_A_PIN) && !gpio_get(BTN_B_PIN)) { // Verifica se ambos estão pressionados (nível baixo)
            uint64_t press_start = time_us_64();
            while (!gpio_get(BTN_A_PIN) && !gpio_get(BTN_B_PIN)) {
                if ((time_us_64() - press_start) >= 3000000) { // 3 segundos mantidos
                    pressed = true;
                    break;
                }
                // Efeito de LED azul piscando
                set_rgb_led(false, false, true);
                sleep_ms(100);
                set_rgb_led(false, false, false);
                sleep_ms(100);
            }
            if (pressed)
                break;
        }
        sleep_ms(10);
    }
    if (!pressed) {
        printf("Falha no passo 3!\n");
        return false;
    }
    play_sound(2000, 1000);
    set_rgb_led(false, true, false);
    sleep_ms(1000);
    set_rgb_led(false, false, false);
    
    return true;
}

// Ativação da máquina
void activate_machine(const char *machine) {
    printf("\nAtivando Máquina %s...\n", machine);
    show_animation(0, 190, 0); // Verde para sucesso
    printf("Maquina %s ativada com sucesso!\n", machine);
}

// Tratamento de falhas
void handle_failed_attempt() {
    failed_attempts++;
    printf("Senha incorreta! Tentativas restantes: %d\n", 3 - failed_attempts);
    effect_password_error();
    if (failed_attempts >= 3) {
        system_locked = true;
        play_sound(300, 2000);
        printf("\nSistema bloqueado! Contate o administrador.\n");
        show_animation(190, 0, 0); // Vermelho para falha
    }
}

// Modo administrador
void admin_mode() {
    printf("\n=== Modo Administrador ===\n");
    printf("Digite a senha especial: ");
    char input[20];
    read_line(input, sizeof(input));
    if (strcmp(input, ADMIN_PASSWORD) != 0) {
        printf("Senha incorreta!\n");
        effect_password_error();
        return;
    }
    effect_password_success();
    printf("\nAcesso concedido!\n");
    printf("Nova senha para Maquina A: ");
    read_line(machine_a_password, sizeof(machine_a_password));
    printf("Nova senha para Maquina B: ");
    read_line(machine_b_password, sizeof(machine_b_password));
    printf("\nSenhas atualizadas com sucesso!\n");
}

// Menu principal
void display_menu() {
    printf("\033[2J\033[H");  // Limpa a tela
    printf("\n=== Menu Principal ===\n");
    printf("1. Usar Maquina A\n");
    printf("2. Usar Maquina B\n");
    printf("3. Modo Administrador\n");
    printf("Escolha uma opcao: ");
}

// Função principal
int main() {
    stdio_init_all();
    setup_adc();
    setup_buttons();
    buzzer_init();
    rgb_led_init();

    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    printf("Sistema de Seguranca Iniciado...\n");
    sleep_ms(10000);

    while (true) {
        if (system_locked) {
            printf("\nPara Destravar! Digite a senha de administrador: ");
            char input[20];
            read_line(input, sizeof(input));
            if (strcmp(input, ADMIN_PASSWORD) == 0) {
                system_locked = false;
                failed_attempts = 0;
                printf("Sistema desbloqueado!\n");
            } else {
                printf("Senha incorreta!\n");
                effect_password_error();
            }
            continue;
        }
        display_menu();
        int choice = read_int();
        switch (choice) {
            case 1:
                if (authenticate_user(machine_a_password)) {
                    effect_password_success();
                    if (perform_safety_steps_machine_A()) {
                        activate_machine("A");
                    } else {
                        show_animation(190, 0, 0); // Vermelho
                    }
                } else {
                    handle_failed_attempt();
                }
                break;
            case 2:
                if (authenticate_user(machine_b_password)) {
                    effect_password_success();
                    if (perform_safety_steps_machine_B()) {
                        activate_machine("B");
                    } else {
                        show_animation(190, 0, 0); // Vermelho
                    }
                } else {
                    handle_failed_attempt();
                }
                break;
            case 3:
                admin_mode();
                break;
            default:
                printf("Opcao invalida!\n");
                break;
        }
    }
    return 0;
}