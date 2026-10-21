/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Jesús David Bonilla Hernández (jebonillah)
 * @brief          : Tarea 01 - Blinky en PH1 con TIM11 (0.5s), Display 7 Segmentos
 *                   3491BS (4 dígitos) con refresco multiplexado en TIM2 a 5 ms,
 *                   limpieza completa del display en cada ciclo (apaga todos
 *                   los LEDs antes de encender el dígito correspondiente)
 *                   y control con EXTI para fotocompuertas (PC13 incrementa,
 *                   PA0 decrementa, operando en uint16_t natural: 0 - 1 = 65535).
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>


// DEFINICIÓN DE VARIABLES GLOBALES Y MÁQUINA DE ESTADOS (FSM)

// Definición de los 2 estados de la máquina de estados
typedef enum {
    ESTADO_ESPERA = 0, // Estado 1: Reposo
    ESTADO_ACTIVO = 1  // Estado 2: Funcionando
} EstadoSistema;

// Variable que almacena el estado actual de la máquina de estados
volatile EstadoSistema estado_actual = ESTADO_ESPERA;

// Banderas activadas por las interrupciones
volatile uint8_t flag_refresco = 0;    // Activada por TIM2
volatile uint8_t flag_incrementar = 0; // Activada por EXTI15_10 (PC13)
volatile uint8_t flag_decrementar = 0; // Activada por EXTI0 (PA0)

// Contador principal del sistema (16 bits sin signo, inicia en 0000)
volatile uint16_t contador = 0;

// Arreglo para almacenar la descomposición en [Miles, Centenas, Decenas, Unidades]
volatile uint8_t digitos[4] = {0, 0, 0, 0};

// Índice del dígito actualmente encendido en el refresco multiplexado (0 a 3)
volatile uint8_t digito_actual = 0;


//CABECERAS DE FUNCIONES

void init_GPIO(void);
void init_timer2(void);
void init_timer11(void);
void init_EXTI(void);

void set_segmentos(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g);
void dibujar_numero(uint8_t numero);
void apagar_digitos(void);
void borrar_display(void);
void activar_digito(uint8_t dig);
void mostrar_numero(uint16_t num);


// FUNCIÓN PRINCIPAL (MAIN)

int main(void){
    // 1. Inicialización de periféricos
    init_GPIO();
    init_timer2();
    init_timer11();
    init_EXTI();

    // 2. Limpieza inicial del display y descomposición del valor inicial (0000)
    borrar_display();
    mostrar_numero(contador);

    // 3. Bucle principal con Máquina de Estados de 2 estados
    while(1){
        switch (estado_actual) {

            case ESTADO_ESPERA:
                // ESTADO 1: Reposo
                break;

            case ESTADO_ACTIVO:

                // A. Atender evento de incremento (Fotocompuerta 1 - PC13)
                if (flag_incrementar) {
                    flag_incrementar = 0;
                    contador++;              // Incremento
                    mostrar_numero(contador);
                }

                // B. Atender evento de decremento (Fotocompuerta 2 - PA0)
                if (flag_decrementar) {
                    flag_decrementar = 0;
                    contador--;              // Decremento 
                    mostrar_numero(contador);
                }

                // C. Atender evento de refresco del display (TIM2)
                if (flag_refresco) {
                    flag_refresco = 0;

                    // 1. Apagar display para evitar efecto fantasma
                    borrar_display();

                    // 2. Dibujar segmentos del dígito correspondiente
                    dibujar_numero(digitos[digito_actual]);

                    // 3. Encender únicamente el dígito actual
                    activar_digito(digito_actual);

                    // 4. Avanzar secuencialmente al siguiente dígito (0 -> 1 -> 2 -> 3 -> 0)
                    digito_actual++;
                    if (digito_actual >= 4) {
                        digito_actual = 0;
                    }
                }

                // Si ya se atendieron todos los eventos pendientes, regresar a reposo
                if (!flag_refresco && !flag_incrementar && !flag_decrementar) {
                    estado_actual = ESTADO_ESPERA;
                }
                break;

            default:
                estado_actual = ESTADO_ESPERA;
                break;
        }
    }

    return 0;
}


// DEFINICIÓN DE FUNCIONES DE CONFIGURACIÓN

/*Asignación de pines:
 * - Blinky:
 *     PH1 -> Salida push-pull (LED parpadeante cada 0.5s con TIM11)
 * - Display 7 segmentos 3491BS:
 *     Segmento A -> PC11 (Activo en 0)
 *     Segmento B -> PC10 (Activo en 0)
 *     Segmento C -> PC8  (Activo en 0)
 *     Segmento D -> PB8  (Activo en 0)
 *     Segmento E -> PC6  (Activo en 0)
 *     Segmento F -> PC12 (Activo en 0)
 *     Segmento G -> PC9  (Activo en 0)
 * - Dígitos (De izquierda a derecha, Activos en nivel BAJO / 0):
 *     Dígito 1 (Miles)    -> PD2
 *     Dígito 2 (Centenas) -> PC5
 *     Dígito 3 (Decenas)  -> PB7
 *     Dígito 4 (Unidades) -> PB9
 * - Fotocompuertas EXTI:
 *     PC13 -> Entrada con Pull-up (Fotocompuerta 1, incrementa contador)
 *     PA0  -> Entrada con Pull-up (Fotocompuerta 2, decrementa contador)
 */

void init_GPIO(void){
    // 1. Habilitar señales de reloj para GPIOA, GPIOB, GPIOC, GPIOD y GPIOH (bus AHB1)
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                     RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                     RCC_AHB1ENR_GPIOHEN);

    // PUERTO H: PH1 (Blinky LED)

    GPIOH->MODER &= ~(GPIO_MODER_MODE1);
    GPIOH->MODER |= (GPIO_MODER_MODE1_0);        // Salida de propósito general (01)
    GPIOH->OTYPER &= ~(GPIO_OTYPER_OT1);         // Salida Push-pull
    GPIOH->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED1);   // Baja velocidad
    GPIOH->PUPDR &= ~(GPIO_PUPDR_PUPDR1);        // Sin resistencias pull-up/pull-down
    GPIOH->ODR &= ~(GPIO_ODR_OD1);               // Inicialmente en bajo

    // PUERTO A: PA0 (Fotocompuerta 2 EXTI0 - Decrementar)

    GPIOA->MODER &= ~(GPIO_MODER_MODE0);         // Modo entrada (00)
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0);       // Pull-Up interna activa

    // PUERTO B: PB7 (Dígito 3), PB8 (Segmento D), PB9 (Dígito 4)

    GPIOB->MODER &= ~(GPIO_MODER_MODE7 | GPIO_MODER_MODE8 | GPIO_MODER_MODE9);
    GPIOB->MODER |= (GPIO_MODER_MODE7_0 | GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0); // Salidas (01)

    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7 | GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);        // Push-pull
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED7 | GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9); // Baja velocidad
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR7 | GPIO_PUPDR_PUPDR8 | GPIO_PUPDR_PUPDR9);  // Sin pull-up/down

    // PB7 y PB9 (Dígitos) inician en ALTO (apagados, lógica activa en bajo)
    // PB8 (Segmento D) inicia en ALTO (apagado, lógica activa en bajo)
    GPIOB->ODR |= (GPIO_ODR_OD7 | GPIO_ODR_OD8 | GPIO_ODR_OD9);

    // PUERTO C:
    // Salidas: PC5 (Dígito 2), PC6 (Seg E), PC8 (Seg C), PC9 (Seg G),
    //          PC10 (Seg B), PC11 (Seg A), PC12 (Seg F)
    // Entrada: PC13 (Fotocompuerta 1 - Incrementar)
    
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
                      GPIO_PUPDR_PUPDR12 | GPIO_PUPDR_PUPDR13);

    GPIOC->PUPDR |= (GPIO_PUPDR_PUPDR13_0);      // Pull-Up en PC13

    // PC5 (Dígito 2) inicia en ALTO (apagado, lógica activa en bajo)
    // Segmentos PC6, PC8, PC9, PC10, PC11, PC12 inician en ALTO (apagados)
    GPIOC->ODR |= (GPIO_ODR_OD5 | GPIO_ODR_OD6 | GPIO_ODR_OD8 | GPIO_ODR_OD9 |
                   GPIO_ODR_OD10 | GPIO_ODR_OD11 | GPIO_ODR_OD12);

    // PUERTO D: PD2 (Dígito 1 - Miles)

    GPIOD->MODER &= ~(GPIO_MODER_MODE2);
    GPIOD->MODER |= (GPIO_MODER_MODE2_0);        // Salida de propósito general (01)
    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT2);         // Push-pull
    GPIOD->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED2);   // Baja velocidad
    GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPDR2);        // Sin pull-up/pull-down
    GPIOD->ODR |= GPIO_ODR_OD2;                  // Inicialmente en ALTO (apagado, lógica activa en bajo)
}

/*
 * Configuración de TIM2 para generar interrupción (refresco de display)
 * Reloj del sistema = 16 MHz
 * APB1 Timer Clock = 16 MHz
 * Con PSC = 16000 - 1, la frecuencia de conteo es 16 MHz / 16000 = 1 kHz (1 tick cada 1 ms)
 * Para 5 ms: ARR = 5 - 1
 */

void init_timer2(void){
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;          // Habilitar reloj para TIM2 (bus APB1)

    TIM2->PSC = 16000 - 1;                       // Prescaler para contar en milisegundos (1 kHz)
    TIM2->ARR = 5 - 1;                           // Auto-reload para 5 ms
    TIM2->CNT = 0;                               // Inicializar contador en 0

    TIM2->CR1 |= TIM_CR1_ARPE;                   // Habilitar auto-reload preload
    TIM2->CR1 &= ~TIM_CR1_DIR;                   // Contador ascendente

    TIM2->SR &= ~TIM_SR_UIF;                     // Limpiar bandera de actualización
    TIM2->DIER |= TIM_DIER_UIE;                  // Habilitar interrupción por actualización (UIE)

    NVIC_EnableIRQ(TIM2_IRQn);              // Habilitar interrupción de TIM2 en el NVIC

    TIM2->CR1 |= TIM_CR1_CEN;                    // Encender el temporizador
}

/*
 * Configuración de TIM11 para generar interrupción cada 0.5 s (500 ms) para Blinky PH1
 * TIM11 está conectado al bus APB2 (Reloj de 16 MHz)
 * Con PSC = 16000 - 1, la frecuencia de conteo es 1 kHz (1 tick cada 1 ms)
 * Para 500 ms: ARR = 500 - 1
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

    NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);// Habilitar interrupción de TIM11 en el NVIC

    TIM11->CR1 |= TIM_CR1_CEN;                   // Encender el temporizador
}

/*
 * Configuración de EXTI para las fotocompuertas:
 * - PC13 (Fotocompuerta 1): Línea 13 (EXTI15_10) incrementa el contador.
 * - PA0 (Fotocompuerta 2): Línea 0 (EXTI0) decrementa el contador.
 */

void init_EXTI(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_SYSCFGEN);
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;        // Habilitar reloj de SYSCFG

    // Configuración EXTI13 en PC13 (Fotocompuerta 1 - Incrementar)

    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13;  // Limpiar selección previa
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC; // Mapear línea 13 a GPIOC (PC13)

    EXTI->RTSR |= EXTI_RTSR_TR13;                // Disparo por flanco de subida
    EXTI->FTSR &= ~EXTI_FTSR_TR13;                 // Sin disparo por flanco de bajada

    EXTI->PR |= EXTI_PR_PR13;                     // Limpiar bandera pendiente escribiendo 1
    EXTI->IMR |= EXTI_IMR_MR13;                   // Desenmascarar interrupción línea 13

    NVIC_EnableIRQ(EXTI15_10_IRQn);          // Habilitar vector EXTI 15 a 10 en NVIC

    // Configuración EXTI0 en PA0 (Fotocompuerta 2 - Decrementar)

    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;   // Limpiar selección previa
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PA; // Mapear línea 0 a GPIOA (PA0)

    EXTI->RTSR &= ~EXTI_RTSR_TR0;                 // Sin disparo por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR0;                  // Disparo por flanco de bajada

    EXTI->PR |= EXTI_PR_PR0;                      // Limpiar bandera pendiente escribiendo 1
    EXTI->IMR |= EXTI_IMR_MR0;                    // Desenmascarar interrupción línea 0

    NVIC_EnableIRQ(EXTI0_IRQn);              // Habilitar vector EXTI 0 en NVIC
}


// CONTROL DEL DISPLAY DE 7 SEGMENTOS (3491BS)

/*
 * Control individual de los 7 segmentos A-G
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

// Dibuja en los segmentos el número solicitado (del 0 al 9)

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

/*
 * Apaga todos los dígitos (nivel ALTO en los pines de dígitos).
 * Pines: Dígito 1 -> PD2, Dígito 2 -> PC5, Dígito 3 -> PB7, Dígito 4 -> PB9
 */

void apagar_digitos(void){
    GPIOD->ODR |= GPIO_ODR_OD2;                  // Dígito 1 (PD2) en 1 (Apagado)
    GPIOC->ODR |= GPIO_ODR_OD5;                  // Dígito 2 (PC5) en 1 (Apagado)
    GPIOB->ODR |= (GPIO_ODR_OD7 | GPIO_ODR_OD9); // Dígitos 3 y 4 (PB7 y PB9) en 1 (Apagados)
}

/*
 * Borra completamente el display apagando todos los dígitos y segmentos:
 * - Dígitos: se ponen en 1 (VCC) para apagarlos (activos en bajo).
 * - Segmentos: se ponen en 1 (VCC) para apagarlos (activos en bajo).
 * Esto asegura que ningún LED quede encendido ni genere efecto fantasma.
 */

void borrar_display(void){
    // 1. Apagar todos los dígitos (todos en 1)
    apagar_digitos();

    // 2. Apagar todos los segmentos (enviar 1 / VCC a los cátodos)
    // PC11(A), PC10(B), PC8(C), PC6(E), PC12(F), PC9(G)
    GPIOC->ODR |= (GPIO_ODR_OD11 | GPIO_ODR_OD10 | GPIO_ODR_OD8 |
                   GPIO_ODR_OD6  | GPIO_ODR_OD12 | GPIO_ODR_OD9);
    // PB8(D)
    GPIOB->ODR |= (GPIO_ODR_OD8);
}

/*
 * Activa el dígito especificado (0 a 3, de izquierda a derecha)
 * asegurando primero que todos los demás dígitos estén apagados.
 *        - El dígito seleccionado se pone en BAJO (0) para encenderlo.
 *        - Los demás dígitos se mantienen en ALTO (1) para permanecer apagados.
 * 0 = Miles (PD2), 1 = Centenas (PC5), 2 = Decenas (PB7), 3 = Unidades (PB9)
 */

void activar_digito(uint8_t dig){
    // 1. Apagar todos los dígitos primero (todos en 1)
    apagar_digitos();

    // 2. Encender únicamente el dígito seleccionado (poner en 0)
    switch(dig){
        case 0:
            GPIOD->ODR &= ~GPIO_ODR_OD2; // Encender Dígito 1 (Miles - PD2 en 0)
            break;
        case 1:
            GPIOC->ODR &= ~GPIO_ODR_OD5; // Encender Dígito 2 (Centenas - PC5 en 0)
            break;
        case 2:
            GPIOB->ODR &= ~GPIO_ODR_OD7; // Encender Dígito 3 (Decenas - PB7 en 0)
            break;
        case 3:
            GPIOB->ODR &= ~GPIO_ODR_OD9; // Encender Dígito 4 (Unidades - PB9 en 0)
            break;
        default:
            break;
    }
}

/*
 * Descompone un número entero sin signo (uint16_t) en unidades, decenas,
 * centenas y miles, y almacena el resultado en el arreglo `digitos`.
 */

void mostrar_numero(uint16_t num){
    // Descomposición posicional decimal:
    digitos[0] = (num / 1000) % 10; // Miles (en 65535 da 5)
    digitos[1] = (num / 100)  % 10; // Centenas (en 65535 da 5)
    digitos[2] = (num / 10)   % 10; // Decenas (en 65535 da 3)
    digitos[3] =  num         % 10; // Unidades (en 65535 da 5)
}


// RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISR)

// ISR de TIM11 (Blinky cada 0.5 s en PH1)

void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF) {
        TIM11->SR &= ~TIM_SR_UIF;        // Limpiar bandera de interrupción de actualización
        GPIOH->ODR ^= GPIO_ODR_OD1;      // Alternar estado de PH1 (Blinky cada 0.5 s)
    }
}

// ISR de TIM2 (Refresco del display 7 segmentos)

void TIM2_IRQHandler(void){
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;         // Limpiar bandera de interrupción de actualización
        flag_refresco = 1;               // Levantar bandera de refresco
        estado_actual = ESTADO_ACTIVO;   // Cambiar la máquina a estado activo
    }
}

// ISR para EXTI línea 0 (PA0 - Fotocompuerta 2: Decrementar)

void EXTI0_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR |= EXTI_PR_PR0;         // Limpiar bandera pendiente escribiendo 1
        flag_decrementar = 1;            // Levantar bandera de decremento
        estado_actual = ESTADO_ACTIVO;   // Despertar la máquina a estado activo
    }
}

// ISR para EXTI líneas 10 a 15 (PC13 - Fotocompuerta 1: Incrementar)

void EXTI15_10_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR13) {
        EXTI->PR |= EXTI_PR_PR13;        // Limpiar bandera pendiente escribiendo 1
        flag_incrementar = 1;            // Levantar bandera de incremento
        estado_actual = ESTADO_ACTIVO;   // Despertar la máquina a estado activo
    }
}