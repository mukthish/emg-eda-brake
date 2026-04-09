/**
 * @file targets/stm32/stm32f4xx.h
 * @brief Minimal STM32F4 register definitions for the Discovery board.
 *
 * This is a simplified subset sufficient for UART, GPIO, and SysTick.
 * For production, use the official CMSIS headers from ST.
 */

#ifndef STM32F4XX_H
#define STM32F4XX_H

#include <stdint.h>

/* ── Cortex-M4 SysTick ──────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

#define SysTick_BASE    0xE000E010UL
#define SysTick         ((SysTick_Type *)SysTick_BASE)

#define SysTick_CTRL_ENABLE     (1UL << 0)
#define SysTick_CTRL_TICKINT    (1UL << 1)
#define SysTick_CTRL_CLKSOURCE  (1UL << 2)

/* ── NVIC ───────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t ISER[8];
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];
    uint32_t RESERVED4[56];
    volatile uint8_t  IP[240];
} NVIC_Type;

#define NVIC_BASE   0xE000E100UL
#define NVIC        ((NVIC_Type *)NVIC_BASE)

#define USART1_IRQn  37

static inline void NVIC_EnableIRQ(int IRQn)
{
    NVIC->ISER[IRQn >> 5] = (1UL << (IRQn & 0x1F));
}

/* ── RCC ────────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    uint32_t RESERVED2;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;

#define RCC_BASE    0x40023800UL
#define RCC         ((RCC_TypeDef *)RCC_BASE)

#define RCC_CR_HSEON        (1UL << 16)
#define RCC_CR_HSERDY       (1UL << 17)
#define RCC_CR_PLLON        (1UL << 24)
#define RCC_CR_PLLRDY       (1UL << 25)

#define RCC_PLLCFGR_PLLSRC_HSE  (1UL << 22)

#define RCC_CFGR_SW_PLL         (2UL << 0)
#define RCC_CFGR_SWS            (3UL << 2)
#define RCC_CFGR_SWS_PLL        (2UL << 2)
#define RCC_CFGR_HPRE_DIV1      (0UL << 4)
#define RCC_CFGR_PPRE1_DIV4     (5UL << 10)
#define RCC_CFGR_PPRE2_DIV2     (4UL << 13)

#define RCC_AHB1ENR_GPIOAEN     (1UL << 0)
#define RCC_AHB1ENR_GPIODEN     (1UL << 3)
#define RCC_APB1ENR_PWREN       (1UL << 28)
#define RCC_APB2ENR_USART1EN    (1UL << 4)

/* ── PWR ────────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CSR;
} PWR_TypeDef;

#define PWR_BASE    0x40007000UL
#define PWR         ((PWR_TypeDef *)PWR_BASE)
#define PWR_CR_VOS  (1UL << 14)

/* ── FLASH ──────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;
} FLASH_TypeDef;

#define FLASH_BASE   0x40023C00UL
#define FLASH        ((FLASH_TypeDef *)FLASH_BASE)

#define FLASH_ACR_LATENCY_5WS   (5UL << 0)
#define FLASH_ACR_PRFTEN        (1UL << 8)
#define FLASH_ACR_ICEN          (1UL << 9)
#define FLASH_ACR_DCEN          (1UL << 10)

/* ── GPIO ───────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

#define GPIOA_BASE  0x40020000UL
#define GPIOD_BASE  0x40020C00UL
#define GPIOA       ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOD       ((GPIO_TypeDef *)GPIOD_BASE)

/* ── USART ──────────────────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_TypeDef;

#define USART1_BASE  0x40011000UL
#define USART1       ((USART_TypeDef *)USART1_BASE)

#define USART_SR_RXNE       (1UL << 5)
#define USART_SR_TXE        (1UL << 7)
#define USART_CR1_UE        (1UL << 13)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_RE        (1UL << 2)
#define USART_CR1_RXNEIE    (1UL << 5)

#endif /* STM32F4XX_H */
