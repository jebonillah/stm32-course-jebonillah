/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Jesus Bonilla (jebonillah)
 * @brief          : Tarea 01 - Blinky PH1 con TIM11, Display 7 Segmentos 3491BS
 *                   (4 dígitos) refrescado con TIM2 a 7 ms y control con EXTI
 *                   (PC13 incrementa, PA0 decrementa). Basado en Taller_exti_ME.
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>

/* ============================================================================
 * DEFINICIÓN DE VARIABLES GLOBALES
 * ============================================================================ */

// Contador principal del sistema (inicia en 0000)
volatile int32_t contador = 0;

// Arreglo para almacenar la descomposición en [Miles, Centenas, Decenas, Unidades]
volatile uint8_t digitos[4] = {0, 0, 0, 0};

// Índice del dígito actualmente encendido en el refresco multiplexado (0 a 3)
volatile uint8_t digito_actual = 0;

/* ============================================================================
 * CABECERAS DE FUNCIONES
 * ============================================================================ */
void init_GPIO(void);
void init_timer2(void);
void init_timer11(void);
void init_EXTI(void);

void set_segmentos(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g);
void dibujar_numero(uint8_t numero);
void apagar_digitos(void);
void activar_digito(uint8_t dig);
void mostrar_numero(int32_t num);

/* ============================================================================
 * FUNCIÓN PRINCIPAL (MAIN)
 * ============================================================================ */
int main(void){
    // Inicialización de periféricos bare metal
    init_GPIO();
    init_timer2();
    init_timer11();
    init_EXTI();

    // Descomponer el número inicial (0000)
    mostrar_numero(contador);

    while(1){
        // Actualizar constantemente la descomposición del contador
        // para que el refresco por interrupción siempre muestre el valor vigente
        mostrar_numero(contador);
    }

    return 0;
}

/* ============================================================================
 * DEFINICIÓN DE FUNCIONES DE CONFIGURACIÓN
 * ============================================================================ */

/**
 * @brief Configuración de puertos GPIO en modo Bare-Metal
 *
 * Asignación de pines:
 * - Blinky:
 *     PH1 -> Salida push-pull (LED parpadeante cada 0.5s)
 * - Display 7 segmentos 3491BS (Ánodo Común):
 *     Segmento A -> PC11
 *     Segmento B -> PC10
 *     Segmento C -> PC8
 *     Segmento D -> PB8
 *     Segmento E -> PC6
 *     Segmento F -> PC12
 *     Segmento G -> PC9
 * - Dígitos (Ánodos comunes de izquierda a derecha):
 *     Dígito 1 (Miles)    -> PD2
 *     Dígito 2 (Centenas) -> PC5
 *     Dígito 3 (Decenas)  -> PB7
 *     Dígito 4 (Unidades) -> PB9
 * - Pulsadores EXTI:
 *     PC13 -> Entrada (Botón usuario B1, flanco de bajada, incrementa)
 *     PA0  -> Entrada con Pull-up (Pulsador externo a GND, flanco de bajada, decrementa)
 */
void init_GPIO(void){
    // 1. Habilitar señales de reloj para GPIOA, GPIOB, GPIOC, GPIOD y GPIOH (bus AHB1)
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                      RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                      RCC_AHB1ENR_GPIOHEN);

    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                     RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                     RCC_AHB1ENR_GPIOHEN);

    // ========================================================================
    // PUERTO H: PH1 (Blinky LED)
    // ========================================================================
    GPIOH->MODER &= ~(GPIO_MODER_MODE1);
    GPIOH->MODER |= (GPIO_MODER_MODE1_0);        // Salida de propósito general

    GPIOH->OTYPER &= ~(GPIO_OTYPER_OT1);         // Salida Push-pull
    GPIOH->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED1);   // Baja velocidad
    GPIOH->PUPDR &= ~(GPIO_PUPDR_PUPDR1);        // Sin resistencias pull-up/pull-down
    GPIOH->ODR &= ~(GPIO_ODR_OD1);               // Inicialmente en bajo

    // ========================================================================
    // PUERTO A: PA0 (Pulsador EXTI - Decrementar)
    // ========================================================================
    GPIOA->MODER &= ~(GPIO_MODER_MODE0);         // Modo entrada (00)
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0);       // Resistencias Pull-Up interna (botón a GND)

    // ========================================================================
    // PUERTO B: PB7 (Dígito 3), PB8 (Segmento D), PB9 (Dígito 4)
    // ========================================================================
    GPIOB->MODER &= ~(GPIO_MODER_MODE7 | GPIO_MODER_MODE8 | GPIO_MODER_MODE9);
    GPIOB->MODER |= (GPIO_MODER_MODE7_0 | GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0); // Salidas

    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7 | GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);        // Push-pull
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED7 | GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9); // Baja velocidad
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR7 | GPIO_PUPDR_PUPDR8 | GPIO_PUPDR_PUPDR9);  // Sin pull-up/down

    // PB7 y PB9 (Dígitos) inician en bajo (apagados en ánodo común)
    GPIOB->ODR &= ~(GPIO_ODR_OD7 | GPIO_ODR_OD9);
    // PB8 (Segmento D) inicia en alto (apagado en ánodo común)
    GPIOB->ODR |= (GPIO_ODR_OD8);

    // ========================================================================
    // PUERTO C:
    // Salidas: PC5 (Dígito 2), PC6 (Seg E), PC8 (Seg C), PC9 (Seg G),
    //          PC10 (Seg B), PC11 (Seg A), PC12 (Seg F)
    // Entrada: PC13 (Pulsador B1 - Incrementar)
    // ========================================================================
    GPIOC->MODER &= ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6 | GPIO_MODER_MODE8 |
                      GPIO_MODER_MODE9 | GPIO_MODER_MODE10 | GPIO_MODER_MODE11 |
                      GPIO_MODER_MODE12 | GPIO_MODER_MODE13);

    GPIOC->MODER |= (GPIO_MODER_MODE5_0 | GPIO_MODER_MODE6_0 | GPIO_MODER_MODE8_0 |
                     GPIO_MODER_MODE9_0 | GPIO_MODER_MODE10_0 | GPIO_MODER_MODE11_0 |
                     GPIO_MODER_MODE12_0); // Configurar salidas (PC13 queda en 00 = Entrada)

    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT5 | GPIO_OTYPER_OT6 | GPIO_OTYPER_OT8 |
                       GPIO_OTYPER_OT9 | GPIO_OTYPER_OT10 | GPIO_OTYPER_OT11 |
                       GPIO_OTYPER_OT12); // Push-pull para salidas

    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED5 | GPIO_OSPEEDR_OSPEED6 | GPIO_OSPEEDR_OSPEED8 |
                         GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED10 | GPIO_OSPEEDR_OSPEED11 |
                         GPIO_OSPEEDR_OSPEED12);

    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR5 | GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR8 |
                      GPIO_PUPDR_PUPDR9 | GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11 |
                      GPIO_PUPDR_PUPDR12 | GPIO_PUPDR_PUPDR13); // Sin pull interno (PC13 tiene pull-up en placa)

    // PC5 (Dígito 2) inicia en bajo (apagado)
    GPIOC->ODR &= ~(GPIO_ODR_OD5);
    // Segmentos PC6, PC8, PC9, PC10, PC11, PC12 inician en alto (apagados en ánodo común)
    GPIOC->ODR |= (GPIO_ODR_OD6 | GPIO_ODR_OD8 | GPIO_ODR_OD9 |
                   GPIO_ODR_OD10 | GPIO_ODR_OD11 | GPIO_ODR_OD12);

    // ========================================================================
    // PUERTO D: PD2 (Dígito 1 - Miles)
    // ========================================================================
    GPIOD->MODER &= ~(GPIO_MODER_MODE2);
    GPIOD->MODER |= (GPIO_MODER_MODE2_0);        // Salida de propósito general

    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT2);         // Push-pull
    GPIOD->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED2);   // Baja velocidad
    GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPDR2);        // Sin pull-up/pull-down
    GPIOD->ODR &= ~(GPIO_ODR_OD2);               // Inicialmente en bajo (apagado)
}

/**
 * @brief Configuración de TIM2 para generar interrupción cada 7 ms (refresco de display)
 * Reloj del sistema = 16 MHz
 * APB1 Timer Clock = 16 MHz
 * Con PSC = 16000 - 1, la frecuencia de conteo es 16 MHz / 16000 = 1 kHz (1 tick cada 1 ms)
 * Para 7 ms: ARR = 7 - 1 = 6
 */
void init_timer2(void){
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;          // Habilitar reloj para TIM2 (bus APB1)

    TIM2->PSC = 16000 - 1;                       // Prescaler para contar en milisegundos (1 kHz)
    TIM2->ARR = 7 - 1;                           // Auto-reload para 7 ms
    TIM2->CNT = 0;                               // Inicializar contador en 0

    TIM2->CR1 |= TIM_CR1_ARPE;                   // Habilitar auto-reload preload
    TIM2->CR1 &= ~TIM_CR1_DIR;                   // Contador ascendente

    TIM2->SR &= ~TIM_SR_UIF;                     // Limpiar bandera de actualización
    TIM2->DIER |= TIM_DIER_UIE;                  // Habilitar interrupción por actualización (UIE)

    NVIC_EnableIRQ(TIM2_IRQn);                   // Habilitar interrupción de TIM2 en el NVIC

    TIM2->CR1 |= TIM_CR1_CEN;                    // Encender el temporizador
}

/**
 * @brief Configuración de TIM11 para generar interrupción cada 0.5 s (500 ms) para Blinky PH1
 * TIM11 está conectado al bus APB2 (Reloj de 16 MHz)
 * Con PSC = 16000 - 1, la frecuencia de conteo es 1 kHz (1 tick cada 1 ms)
 * Para 500 ms: ARR = 500 - 1 = 499
 */
void init_timer11(void){
    RCC->APB2ENR &= ~RCC_APB2ENR_TIM11EN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;         // Habilitar reloj para TIM11 (bus APB2)

    TIM11->PSC = 16000 - 1;                      // Prescaler para contar en milisegundos (1 kHz)
    TIM11->ARR = 500 - 1;                        // Auto-reload para 500 ms (0.5 s)
    TIM11->CNT = 0;                              // Inicializar contador en 0

    TIM11->CR1 |= TIM_CR1_ARPE;                  // Habilitar auto-reload preload

    TIM11->SR &= ~TIM_SR_UIF;                    // Limpiar bandera de actualización
    TIM11->DIER |= TIM_DIER_UIE;                 // Habilitar interrupción de actualización

    NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);     // Habilitar interrupción de TIM11 en el NVIC

    TIM11->CR1 |= TIM_CR1_CEN;                   // Encender el temporizador
}

/**
 * @brief Configuración de EXTI para los pulsadores:
 * - PC13 (Botón azul Nucleo): Genera interrupción en la línea 13 (EXTI15_10) e incrementa el contador.
 * - PA0: Genera interrupción en la línea 0 (EXTI0) y decrementa el contador.
 */
void init_EXTI(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_SYSCFGEN);
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;        // Habilitar reloj de SYSCFG

    // ------------------------------------------------------------------------
    // Configuración EXTI13 en PC13 (Incrementar)
    // ------------------------------------------------------------------------
    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13; // Limpiar selección previa
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC;// Mapear línea 13 a GPIOC (PC13)

    EXTI->RTSR &= ~EXTI_RTSR_TR13;               // Sin disparo por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR13;                // Disparo por flanco de bajada (al presionar a GND)

    EXTI->PR |= EXTI_PR_PR13;                    // Limpiar bandera pendiente escribiendo 1
    EXTI->IMR |= EXTI_IMR_MR13;                  // Desenmascarar interrupción línea 13

    NVIC_EnableIRQ(EXTI15_10_IRQn);              // Habilitar vector EXTI 15 a 10 en NVIC

    // ------------------------------------------------------------------------
    // Configuración EXTI0 en PA0 (Decrementar)
    // ------------------------------------------------------------------------
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;  // Limpiar selección previa (selecciona PA0)
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PA;// Mapear línea 0 a GPIOA (PA0)

    EXTI->RTSR &= ~EXTI_RTSR_TR0;                // Sin disparo por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR0;                 // Disparo por flanco de bajada (pulsador a GND con Pull-Up)

    EXTI->PR |= EXTI_PR_PR0;                     // Limpiar bandera pendiente escribiendo 1
    EXTI->IMR |= EXTI_IMR_MR0;                   // Desenmascarar interrupción línea 0

    NVIC_EnableIRQ(EXTI0_IRQn);                  // Habilitar vector EXTI 0 en NVIC
}

/* ============================================================================
 * CONTROL DEL DISPLAY DE 7 SEGMENTOS (3491BS - ÁNODO COMÚN)
 * ============================================================================ */

/**
 * @brief Control individual de los 7 segmentos A-G
 * Al ser un display de Ánodo Común (3491BS):
 * - Para encender el segmento (1 lógico recibido) se envía nivel BAJO (0 / GND al cátodo).
 * - Para apagar el segmento (0 lógico recibido) se envía nivel ALTO (1 / VCC al cátodo).
 *
 * Mapeo de pines de segmentos:
 * A -> PC11, B -> PC10, C -> PC8, D -> PB8, E -> PC6, F -> PC12, G -> PC9
 */
void set_segmentos(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g){
    // Segmento A -> PC11
    if (a) GPIOC->ODR &= ~GPIO_ODR_OD11; else GPIOC->ODR |= GPIO_ODR_OD11;

    // Segmento B -> PC10
    if (b) GPIOC->ODR &= ~GPIO_ODR_OD10; else GPIOC->ODR |= GPIO_ODR_OD10;

    // Segmento C -> PC8
    if (c) GPIOC->ODR &= ~GPIO_ODR_OD8;  else GPIOC->ODR |= GPIO_ODR_OD8;

    // Segmento D -> PB8
    if (d) GPIOB->ODR &= ~GPIO_ODR_OD8;  else GPIOB->ODR |= GPIO_ODR_OD8;

    // Segmento E -> PC6
    if (e) GPIOC->ODR &= ~GPIO_ODR_OD6;  else GPIOC->ODR |= GPIO_ODR_OD6;

    // Segmento F -> PC12
    if (f) GPIOC->ODR &= ~GPIO_ODR_OD12; else GPIOC->ODR |= GPIO_ODR_OD12;

    // Segmento G -> PC9
    if (g) GPIOC->ODR &= ~GPIO_ODR_OD9;  else GPIOC->ODR |= GPIO_ODR_OD9;
}

/**
 * @brief Dibuja en los segmentos el número solicitado (del 0 al 9)
 * @param numero Valor numérico entre 0 y 9
 */
void dibujar_numero(uint8_t numero){
    switch(numero){
        case 0:
            // A, B, C, D, E, F encendidos; G apagado
            set_segmentos(1, 1, 1, 1, 1, 1, 0);
            break;
        case 1:
            // B, C encendidos
            set_segmentos(0, 1, 1, 0, 0, 0, 0);
            break;
        case 2:
            // A, B, D, E, G encendidos
            set_segmentos(1, 1, 0, 1, 1, 0, 1);
            break;
        case 3:
            // A, B, C, D, G encendidos
            set_segmentos(1, 1, 1, 1, 0, 0, 1);
            break;
        case 4:
            // B, C, F, G encendidos
            set_segmentos(0, 1, 1, 0, 0, 1, 1);
            break;
        case 5:
            // A, C, D, F, G encendidos
            set_segmentos(1, 0, 1, 1, 0, 1, 1);
            break;
        case 6:
            // A, C, D, E, F, G encendidos
            set_segmentos(1, 0, 1, 1, 1, 1, 1);
            break;
        case 7:
            // A, B, C encendidos
            set_segmentos(1, 1, 1, 0, 0, 0, 0);
            break;
        case 8:
            // Todos los segmentos encendidos
            set_segmentos(1, 1, 1, 1, 1, 1, 1);
            break;
        case 9:
            // A, B, C, D, F, G encendidos
            set_segmentos(1, 1, 1, 1, 0, 1, 1);
            break;
        default:
            // Cualquier otro caso: todos apagados
            set_segmentos(0, 0, 0, 0, 0, 0, 0);
            break;
    }
}

/**
 * @brief Apaga todos los dígitos (ánodos comunes) para evitar sombras/fantasmas (ghosting)
 * Pines: Dígito 1 -> PD2, Dígito 2 -> PC5, Dígito 3 -> PB7, Dígito 4 -> PB9
 * En ánodo común, nivel bajo (0) desconecta el ánodo del dígito.
 */
void apagar_digitos(void){
    GPIOD->ODR &= ~GPIO_ODR_OD2; // Dígito 1 (PD2) en 0
    GPIOC->ODR &= ~GPIO_ODR_OD5; // Dígito 2 (PC5) en 0
    GPIOB->ODR &= ~GPIO_ODR_OD7; // Dígito 3 (PB7) en 0
    GPIOB->ODR &= ~GPIO_ODR_OD9; // Dígito 4 (PB9) en 0
}

/**
 * @brief Activa el ánodo del dígito especificado (0 a 3, de izquierda a derecha)
 * @param dig 0 = Miles (PD2), 1 = Centenas (PC5), 2 = Decenas (PB7), 3 = Unidades (PB9)
 */
void activar_digito(uint8_t dig){
    apagar_digitos();
    switch(dig){
        case 0:
            GPIOD->ODR |= GPIO_ODR_OD2; // Encender Dígito 1 (Miles)
            break;
        case 1:
            GPIOC->ODR |= GPIO_ODR_OD5; // Encender Dígito 2 (Centenas)
            break;
        case 2:
            GPIOB->ODR |= GPIO_ODR_OD7; // Encender Dígito 3 (Decenas)
            break;
        case 3:
            GPIOB->ODR |= GPIO_ODR_OD9; // Encender Dígito 4 (Unidades)
            break;
        default:
            break;
    }
}

/**
 * @brief Descompone un número entero en unidades, decenas, centenas y miles,
 *        y almacena el resultado en el arreglo `digitos` para ser refrescado.
 * @param num Número a descomponer y mostrar
 */
void mostrar_numero(int32_t num){
    // Usamos el valor absoluto para descomponer de forma segura incluso si el contador es negativo
    uint32_t temp = (num < 0) ? (uint32_t)(-num) : (uint32_t)num;

    // Descomposición posicional:
    digitos[0] = (temp / 1000) % 10; // Miles (Dígito 1, izquierda)
    digitos[1] = (temp / 100)  % 10; // Centenas (Dígito 2)
    digitos[2] = (temp / 10)   % 10; // Decenas (Dígito 3)
    digitos[3] =  temp         % 10; // Unidades (Dígito 4, derecha)
}

/* ============================================================================
 * RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISR)
 * ============================================================================ */

/**
 * @brief ISR de TIM11 (Blinky cada 0.5 s en PH1)
 * Comparte el vector TIM1_TRG_COM_TIM11_IRQHandler
 */
void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF) {
        TIM11->SR &= ~TIM_SR_UIF;        // Limpiar bandera de interrupción de actualización
        GPIOH->ODR ^= GPIO_ODR_OD1;      // Alternar estado de PH1 (Blinky cada 0.5 s)
    }
}

/**
 * @brief ISR de TIM2 (Refresco del display 7 segmentos cada 7 ms)
 * Realiza el barrido multiplexado activando secuencialmente cada uno de los 4 dígitos
 */
void TIM2_IRQHandler(void){
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;         // Limpiar bandera de interrupción de actualización

        // 1. Apagar todos los dígitos para evitar ghosting
        apagar_digitos();

        // 2. Colocar los segmentos del número correspondiente al dígito actual
        dibujar_numero(digitos[digito_actual]);

        // 3. Encender el ánodo del dígito actual
        activar_digito(digito_actual);

        // 4. Pasar al siguiente dígito de izquierda a derecha (0 -> 1 -> 2 -> 3 -> 0)
        digito_actual++;
        if (digito_actual >= 4) {
            digito_actual = 0;
        }
    }
}

/**
 * @brief ISR para EXTI línea 0 (PA0)
 * Al presionar el pulsador conectado a PA0, decrementa el contador en 1
 */
void EXTI0_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR |= EXTI_PR_PR0;         // Limpiar bandera pendiente escribiendo 1
        contador--;                      // Decrementar contador
    }
}

/**
 * @brief ISR para EXTI líneas 10 a 15 (PC13)
 * Al presionar el botón de usuario B1 (PC13), incrementa el contador en 1
 */
void EXTI15_10_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR13) {
        EXTI->PR |= EXTI_PR_PR13;        // Limpiar bandera pendiente escribiendo 1
        contador++;                      // Incrementar contador
    }
}
