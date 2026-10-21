#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>

// Cabecera de funciones
void init_GPIO(void);
void init_timer11(void);
void init_timer3_pwm(void);

// Main
int main(void){
    init_GPIO();
    init_timer11();
    init_timer3_pwm();

    while(1){
        // El parpadeo de PH1 se gestiona en la interrupción de TIM11 (cada 500 ms)
        // La señal PWM en PA6 es generada autónomamente por hardware con TIM3 (30 kHz, 50% duty)
    }
    return 0;
}

// Definicion de funciones

void init_GPIO(void){
    // Habilitar reloj para los puertos GPIOA y GPIOH
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOHEN);
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOHEN);

    // ==========================================
    // Puerto H - Pin 1 (Blinky LED cada 500 ms)
    // ==========================================
    GPIOH->MODER &= ~(GPIO_MODER_MODE1); // Limpiar bits de modo para PH1
    GPIOH->MODER |= (GPIO_MODER_MODE1_0); // Configurar PH1 como salida (general purpose output)

    GPIOH->OTYPER &= ~(GPIO_OTYPER_OT1); // Configurar PH1 como salida push-pull

    GPIOH->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED1); // Configurar PH1 como baja velocidad

    GPIOH->PUPDR &= ~(GPIO_PUPDR_PUPDR1); // Configurar PH1 sin pull-up/pull-down

    GPIOH->ODR &= ~(GPIO_ODR_OD1); // Inicializar PH1 en bajo

    // ==========================================
    // Puerto A - Pin 6 (Salida PWM TIM3_CH1 en Función Alternativa AF02)
    // ==========================================
    GPIOA->MODER &= ~(GPIO_MODER_MODE6); // Limpiar bits de modo para PA6
    GPIOA->MODER |= (GPIO_MODER_MODE6_1); // Configurar PA6 como función alternativa (10)

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT6); // Configurar PA6 como salida push-pull

    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED6);
    GPIOA->OSPEEDR |= (GPIO_OSPEEDR_OSPEED6_1); // Configurar PA6 como alta velocidad

    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR6); // Configurar PA6 sin pull-up/pull-down

    // Seleccionar función alternativa AF02 (TIM3_CH1) en AFR[0] (AFR Low)
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL6); // Limpiar selección de función alternativa para PA6
    GPIOA->AFR[0] |= (2U << GPIO_AFRL_AFSEL6_Pos); // Asignar AF02 (TIM3_CH1)
}

void init_timer11(void){
    // TIM11 está conectado al bus APB2
    RCC->APB2ENR &= ~RCC_APB2ENR_TIM11EN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;

    TIM11->PSC = 16000 - 1; // Prescaler para contar en milisegundos (16 MHz / 16000 = 1 kHz)
    TIM11->ARR = 500 - 1;   // Auto-reload para 500 ms (medio segundo)
    TIM11->CNT = 0;         // Inicializar el contador en 0

    TIM11->CR1 |= TIM_CR1_ARPE; // Habilitar auto-reload preload
    TIM11->CR1 &= ~TIM_CR1_DIR; // Contador ascendente

    TIM11->SR &= ~TIM_SR_UIF;   // Limpiar la bandera de actualización

    TIM11->DIER |= TIM_DIER_UIE; // Habilitar la interrupción de actualización (Update Interrupt)

    NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn); // Habilitar la interrupción de TIM11 en el NVIC

    TIM11->CR1 |= TIM_CR1_CEN;  // Habilitar el contador
}

void init_timer3_pwm(void){
    // Habilitar reloj para TIM3 (bus APB1)
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM3EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Frecuencia deseada = 30 kHz
    // f_TIM3 = 16 MHz
    // ARR = (16 MHz / ( (PSC + 1) * f_PWM )) - 1
    // Con PSC = 0: ARR = (16000000 / 30000) - 1 = 533 - 1 = 532
    // Frecuencia real resultante = 16 MHz / 533 = 30018.76 Hz (~30.02 kHz)
    TIM3->PSC = 0;          // Prescaler en 0 (reloj de conteo a 16 MHz)
    TIM3->ARR = 533 - 1;    // Periodo para 30 kHz (ARR = 532)

    // Duty cycle = 50%
    // CCR1 = (ARR + 1) * 50% = 533 / 2 = 266
    TIM3->CCR1 = 266;

    // Configuración del canal 1 en modo PWM 1
    TIM3->CCMR1 &= ~TIM_CCMR1_CC1S; // Configurar canal 1 como salida (CC1S = 00)
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M; // Limpiar bits de modo de salida OC1M
    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); // Modo PWM 1 (110: activo mientras CNT < CCR1)
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; // Habilitar precarga del registro CCR1 (Preload Enable)

    // Habilitar salida en el pin para el canal 1 (polaridad activa en alto por defecto)
    TIM3->CCER |= TIM_CCER_CC1E;

    // Habilitar auto-reload preload
    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM3->CR1 &= ~TIM_CR1_DIR; // Contador ascendente

    // Generar un evento de actualización para cargar inmediatamente los registros de sombra
    TIM3->EGR |= TIM_EGR_UG;
    TIM3->SR &= ~TIM_SR_UIF; // Limpiar la bandera generada por el evento UG

    // Habilitar el temporizador TIM3
    TIM3->CR1 |= TIM_CR1_CEN;
}

// Rutina de servicio de interrupción para TIM11 (comparte vector con TIM1 TRG/COM)
void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF) { // Verificar si la interrupción fue por actualización
        TIM11->SR &= ~TIM_SR_UIF; // Limpiar la bandera de actualización

        GPIOH->ODR ^= GPIO_ODR_OD1; // Alternar estado del pin PH1 cada 500 ms
    }
}
