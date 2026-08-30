/* Unsigned Integer Types */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

/* Signed Integer Types */
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

/* Floating-Point Types */
typedef float               float32_t;
typedef double              float64_t;

/* Base Addresses */
#define PERIPH_BASE     (0x40000000U)
#define APB1PERIPH_BASE (PERIPH_BASE)
#define APB2PERIPH_BASE (PERIPH_BASE + 0x10000U)
#define AHB1PERIPH_BASE (PERIPH_BASE + 0x20000U)
#define AHB2PERIPH_BASE (PERIPH_BASE + 0x8000000U)
#define DFSDM1_BASE     (APB2PERIPH_BASE + 0x6000U)
#define UART4_BASE      (APB1PERIPH_BASE + 0x4C00U)
#define RCC_BASE        (AHB1PERIPH_BASE + 0x1000U)
#define GPIOA_BASE      (AHB2PERIPH_BASE + 0U)
#define GPIOE_BASE      (AHB2PERIPH_BASE + 0x1000U)
#define SYSTICK_BASE    (0xE000E010U)

/* RCC Register Addresses */
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE))
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x58U))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x60U))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x4CU))

/* UART Register Addresses*/
#define USART_CR1       (*(volatile uint32_t *)(UART4_BASE))
#define USART_CR2       (*(volatile uint32_t *)(UART4_BASE + 0x4U))
#define USART_BRR       (*(volatile uint32_t *)(UART4_BASE + 0xCU))
#define USART_ISR       (*(volatile uint32_t *)(UART4_BASE + 0x1CU))
#define USART_TDR       (*(volatile uint32_t *)(UART4_BASE + 0x28U))

/* DFSDM Register Addresses */
#define CH0CFGR1        (*(volatile uint32_t *)(DFSDM1_BASE))
#define CH2CFGR1        (*(volatile uint32_t *)(DFSDM1_BASE + 0x40U))
#define FLT0CR1         (*(volatile uint32_t *)(DFSDM1_BASE + 0x100U))
#define FLT0ISR         (*(volatile uint32_t *)(DFSDM1_BASE + 0x108U))
#define FLT0ICR         (*(volatile uint32_t *)(DFSDM1_BASE + 0x10CU))
#define FLT0FCR         (*(volatile uint32_t *)(DFSDM1_BASE + 0x114U))
#define FLT0RDATAR      (*(volatile uint32_t *)(DFSDM1_BASE + 0x11CU))

/* SysTick Timer Addresses */
#define STK_CTRL        (*(volatile uint32_t *)(SYSTICK_BASE))
#define STK_LOAD        (*(volatile uint32_t *)(SYSTICK_BASE + 0x4U))
#define STK_VAL         (*(volatile uint32_t *)(SYSTICK_BASE + 0x8U))
#define STK_CALIB       (*(volatile uint32_t *)(SYSTICK_BASE + 0xCU))

/* GPIO Peripheral Register Struct */
typedef struct
{
    volatile uint32_t MODER;    // 0x00
    volatile uint32_t OTYPER;   // 0x04
    volatile uint32_t OSPEEDR;  // 0x08
    volatile uint32_t PUPDR;    // 0x0C
    volatile uint32_t IDR;      // 0x10
    volatile uint32_t ODR;      // 0x14
    volatile uint32_t BSRR;     // 0x18
    volatile uint32_t LCKR;     // 0x1C
    volatile uint32_t AFRL;     // 0x20
    volatile uint32_t AFRH;     // 0x24
    volatile uint32_t BRR;      // 0x28
    volatile uint32_t ASCR;     // 0x2C
} GPIO_TypeDef;

/* GPIOA */
#define GPIOA           ((GPIO_TypeDef *)(GPIOA_BASE))
#define GPIOA_CLOCK     (1 << 0)
#define PA0_MASK        (0xFFFFFFFCU)
#define UART_OUTPUT     (1 << 1)

/* GPIOE */
#define GPIOE           ((GPIO_TypeDef *)(GPIOE_BASE))
#define GPIOE_CLOCK     (1 << 4)
#define PE9_MASK        (0xFFF3FFFFU)
#define MIC_CLOCK       (1 << 19)
#define PE7_MASK        (0xFFFF3FFFU)
#define MIC_DATA        (1 << 15)

/* MSI FREQUENCY */
#define MSI_MASK        (0xFFFFFF0FU)
#define MSI_16MHZ       (1 << 7)
#define CLK_RANGE_SEL   (1 << 3)
#define MSI_CLK_SEL     (MSI_16MHZ | CLK_RANGE_SEL)

/* UART */
#define UART4_EN        (1 << 19)
#define BAUD_DIV_921600 (17) // Clock Hz (16 MHz) / 921600 ≈ 17.36 → 17
#define WORD_LENGTH_1   (0 << 28) // M1
#define WORD_LENGTH_0   (0 << 12) // M0
#define WORD_LENGTH     (WORD_LENGTH_1 | WORD_LENGTH_0)
#define TX_ENABLE       (1 << 3)
#define WORD_LEN_TX_EN  (WORD_LENGTH | TX_ENABLE)
#define UART_ENABLE     (1 << 0)
#define STOP_1          (1 << 13)
#define STOP_2          (0 << 12)
#define STOP_BITS       (STOP_1 | STOP_2)
#define UART_TXE        (0x80U)

/* Alternate Function Register Low */
#define AF8_UART4       (1 << 3)
#define AF6_MIC_BIT_30  (1 << 30)
#define AF6_MIC_BIT_29  (1 << 29)
#define AF6_MIC_PE7     (AF6_MIC_BIT_30 | AF6_MIC_BIT_29)

/* Alternate Function Register High */
#define AF6_MIC_BIT_6   (1 << 6)
#define AF6_MIC_BIT_5   (1 << 5)
#define AF6_MIC_PE9     (AF6_MIC_BIT_6 | AF6_MIC_BIT_5)

/* SysTick Control and Status Register */
#define AHB_CLOCK       (1 << 2)
#define COUNTER_ENABLE  (1)
#define TICK_TIME       (0x3E7FU) // 15999
#define TICK_INTERRUPT  (1 << 1)
#define CLK_SOURCE_INT  (AHB_CLOCK | TICK_INTERRUPT)

/* MIC */
#define DFSDM1EN        (1 << 24) // DFSDM1 clock enabled
#define CKOUTDIV        (0x7U << 16) // divides output clock, bits [23:16]
#define DFSDMEN         (1 << 31) // DFSDM globally enabled
#define CLK_DIV_DFSDMEN (CKOUTDIV | DFSDMEN)
#define CHEN            (1 << 7) // channel enable
#define SPICKSEL        (1 << 2) // bits = 01
#define CHEN_SPICKSEL   (CHEN | SPICKSEL)
#define RCH             (1 << 25) // bits [26:24] = 010
#define DFEN            (1 << 0) // digital filter enable
#define SINC_3_FILTER   (3 << 29)
#define FOSR            (63 << 16)
#define SINC_3_FOSR     (SINC_3_FILTER | FOSR)
#define RCONT           (1 << 18)
#define RCH_DFEN_RCONT  (RCH | DFEN | RCONT)
#define RSWSTART        (1 << 17)
#define REOCF           (1 << 1)

/* Analyze Mic Data */
#define DEBOUNCE_VAL    (20)
#define CAPACITY        (256)
#define ENERGY_LOUD     (1500)
#define REARM_DELAY     (500)

uint32_t timer = 0; // defined globally since it's used for interrupts
uint16_t rearm_delay = REARM_DELAY;
int32_t sample_buffer[CAPACITY]; // 1024 bytes total (y = 4 bytes)
static uint16_t index = 0;
static uint16_t loud_count = 0;
static uint16_t low_count = 0;
static uint8_t capturing = 0;

struct DC_Blocker
{
    float R;      // filter coefficient
    float x_prev; // previous input
    float y_prev; // previous output
};
struct DC_Blocker dc = {0.995, 0, 0}; // initializing DC_Blocker

struct Update_Energy
{
    uint32_t energy;
    float alpha;
};
struct Update_Energy energy = {0, 0.2};

void UART_Setup(void)
{
    RCC_APB1ENR1 |= UART4_EN; // enable clock

    USART_CR1 |= WORD_LEN_TX_EN; // 1 start bit, 8 data bits; enables TX
    USART_CR2 |= STOP_BITS; // 2 stop bits

    RCC_CR &= MSI_MASK; // clear MSI bits
    RCC_CR |= MSI_CLK_SEL; // set MSI to 16 MHz, sets MSI = 1000;  use MSI range provided in RCC_CR (16 MHz)

    USART_BRR = BAUD_DIV_921600;

    USART_CR1 |= UART_ENABLE;
}

void UART4_GPIO_Init(void)
{
    RCC_AHB2ENR |= GPIOA_CLOCK; // enables GPIOA clock
    GPIOA->MODER &= PA0_MASK; // clears bits [1:0]
    GPIOA->MODER |= UART_OUTPUT; // alternate fucntion mode (UART)
    GPIOA->AFRL = AF8_UART4; // sets alternate function for PA0
}

void UART_Transmit(char buff)
{
    while (!(USART_ISR & UART_TXE)); // waits until data is transferred to shift register, waits until TXE bit = 1
    USART_TDR = buff; // sets Transmit Data Register = buff
}

void UART_Transmit_Ptr(unsigned char *buff, uint8_t setting)
{
    uint8_t i;

    if (setting == 1)
    {
        UART_Transmit(*buff);
    }

    else if (setting == 2)
    {
        for (i = 0; i < 2; i++) // runs two times
        {
            UART_Transmit(*buff);
            buff++;
        }
    }

    else if (setting == 3)
    {
        for (i = 0; i < 3; i++) // runs three times
        {
            UART_Transmit(*buff);
            buff++;
        }
    }

    else if (setting == 4)
    {
        for (i = 0; i < 4; i++) // runs four times
        {
            UART_Transmit(*buff);
            buff++;
        }
    }
}

void Convert_Uint_To_Bytes(uint32_t value, uint8_t setting) // uint32_t, big endian
{
    if (setting == 1)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }

    else if (setting == 2)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) ((value & 0x0000FF00UL) >> 8);  // LSR 1 byte
        buffer[1] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }

    else if (setting == 4)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) ((value & 0xFF000000UL) >> 24); // LSR 3 bytes
        buffer[1] = (unsigned char) ((value & 0x00FF0000UL) >> 16); // LSR 2 bytes
        buffer[2] = (unsigned char) ((value & 0x0000FF00UL) >> 8);  // LSR 1 byte
        buffer[3] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }
}

void Convert_Int_To_Bytes(int32_t value, uint8_t setting) // int32_t, big endian
{
    if (setting == 1)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }

    else if (setting == 2)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) ((value & 0x0000FF00UL) >> 8);  // LSR 1 byte
        buffer[1] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }

    else if (setting == 4)
    {
        unsigned char buffer[4]; // buffer = 4 bytes

        buffer[0] = (unsigned char) ((value & 0xFF000000UL) >> 24); // LSR 3 bytes
        buffer[1] = (unsigned char) ((value & 0x00FF0000UL) >> 16); // LSR 2 bytes
        buffer[2] = (unsigned char) ((value & 0x0000FF00UL) >> 8);  // LSR 1 byte
        buffer[3] = (unsigned char) (value  & 0x000000FFUL);
        UART_Transmit_Ptr(buffer, setting);
    }
}

void Send_Sample_Buffer(void)
{
    uint8_t setting = 4;
    uint16_t index_counter;

    /* 4-byte markers — 1-byte 0xFF collides with negative PCM MSB */
    Convert_Uint_To_Bytes(0xFFFFFFFFUL, setting);

    for (index_counter = 0; index_counter < CAPACITY; index_counter++)
    {
        Convert_Int_To_Bytes(sample_buffer[index_counter], setting);
    }

    Convert_Uint_To_Bytes(0x00000000UL, setting);
}

// Capture exactly CAPACITY PCM samples after the first time energy > 1000
// After CAPACITY samples, keep/send only if loud_count >= DEBOUNCE_VAL
void Sound_Detect(uint32_t energy, int32_t y)
{
    if (timer < 7000) // disable capture for first 7000ms
    {
        capturing = 0;
        index = 0;
        loud_count = 0;
        low_count = 0;
        UART_Transmit(0);
        return;
    }

    if (rearm_delay > 0)
    {
        if (energy <= ENERGY_LOUD)
        {
            rearm_delay--;
        }

        UART_Transmit(0);
        return;
    }

    if (!capturing) // captures after first loud sample (> 1000)
    {
        if (energy > ENERGY_LOUD)
        {
            // initialize variables/flags
            capturing = 1;
            index = 0;
            loud_count = 0;
            low_count = 0;
        }
        else
        {
            UART_Transmit(0);
            return;
        }
    }

    sample_buffer[index] = y; // store PCM y for all CAPACITY samples

    if (energy > ENERGY_LOUD) 
    {
        loud_count++; // if energy > 1000, increment loud_count AND index
        low_count = 0; // reset low_count
    }
    else low_count++; // if energy <= 1000, increment low_count

    index++;

    if ((low_count > 750)  && (loud_count < DEBOUNCE_VAL))// if low_count > 250 AND loud_count < 30, restart sampling
    {
        // re-initialize variables/flags
        index = 0;
        capturing = 0;
        loud_count = 0;
        low_count = 0;
    }

    if (index >= CAPACITY) // if index reaches 256, index max = 255
    {
        if (loud_count >= DEBOUNCE_VAL) // tracks the amount of val > 1000
        {
            Send_Sample_Buffer(); // send sample_buffer
        }

        // reset variables/flags
        capturing = 0;
        index = 0;
        loud_count = 0;
        low_count = 0;
        rearm_delay = REARM_DELAY;
    }

    UART_Transmit((loud_count >= DEBOUNCE_VAL) ? 1 : 0);
}

void Update_Energy(struct Update_Energy *select, int32_t y)
{
    uint8_t setting = 4;

    uint32_t abs_y; // absolute y
    if (y < 0) abs_y = -y; // take magnitude (not direction) of y
    else abs_y = y;

    uint32_t val_1 = select->alpha * abs_y; // val_1 = a*abs_y
    uint32_t val_2 = (1 - select->alpha) * select->energy; // val_2 = (1-a)*energy
    select->energy = val_1 + val_2; // energy = val_1 + val_2

    Convert_Uint_To_Bytes(select->energy, setting);
    Sound_Detect(select->energy, y);
}

void DC_Blocker(struct DC_Blocker *select, int32_t x)
{
    uint8_t setting = 4;

    int32_t y = x  - select->x_prev + (select->R)*(select->y_prev); // y = x - x_prev + R*y_prev
    select->x_prev = x; // x_prev = x
    select->y_prev = y; // y_prev = y
    Convert_Int_To_Bytes(y, setting); // pass in 32-bit mic_data number
    Update_Energy(&energy, y); // call update energy function; pass in energy struct and y
}

void Mic_Setup(void)
{
    RCC_AHB2ENR |= GPIOE_CLOCK; // enables GPIOE clock

    /* PE9: Clock */
    GPIOE->MODER &= PE9_MASK; // clear bits [19:18]
    GPIOE->MODER |= MIC_CLOCK; // alternate function mode
    GPIOE->AFRH |= AF6_MIC_PE9; // sets alternate function for PE9

    /* PE7: Data */
    GPIOE->MODER &= PE7_MASK; // clear bits [15:14]
    GPIOE->MODER |= MIC_DATA; // alternate function mode
    GPIOE->AFRL |= AF6_MIC_PE7; // sets alternate function for PE9
}

void DFSDM_Setup(void)
{
    RCC_APB2ENR |= DFSDM1EN; // DFSDM1 clock enable

    /*
        VAL + 1 = CLK / OUTPUT_CLK

        CLK: 16 MHz, OUTPUT_CLK: 2 MHz
        VAL = 8 - 1 = 7
    */
    CH0CFGR1 |= CLK_DIV_DFSDMEN; // divides output clock; enable DFSDM interface

    CH2CFGR1 |= CHEN_SPICKSEL; // channel 2 enable, PE7; clock comes from internal CKOUT output

    /*
        SINC_3: Sinc 3 filter, usual default for PDM mics
        FOSR: +/-262144 (FOSR^3 for Sinc 3)

        FOSR val  = FOSR bits[9:0] + 1
    */
    FLT0FCR |= SINC_3_FOSR; // Sinc 3: usual default for PDM mics

    /*
        RCH: selected channel 2
        DFEN: digital filter 0 enable
        RCONT: channel converted repeatedly after each conversion request
    */
    FLT0CR1 |= RCH_DFEN_RCONT;

    /* Wait for digital filter to be enabled (DFEN), then set RSWSTART = 1 */
    FLT0CR1 |= RSWSTART; // makes a request to start conversion on the regular channel
}

void Get_Mic_Sample(void)
{
    while (!(FLT0ISR & REOCF)); // wait until a regular conversion is done

    /* When data register FLT0RDATAR is read, REOCF bit is cleared automatically */
    int32_t mic_data = FLT0RDATAR; // mic_data = bits[31:8] of FLT0RDATAR register
    DC_Blocker(&dc, mic_data >> 8); // shift by 1 byte
}

void SysTick_Init(void)
{
    STK_CTRL |= CLK_SOURCE_INT; // sets clock source to 16 MHz, counting down to 0 enables interrupt 

    /*  
        MSI (16 MHz)
        SYSCLK (default = MSI)
        divide HPRE (default = 1)
        HCLK (16 MHz) 
        TICK_TIME = (HCLK / 1000) - 1

        1000 = CLOCKS/SEC -> 1 CLK every 1 ms
    */
    STK_LOAD |= TICK_TIME; // 1 ms increment (time to count down from 15999 to 0)
    STK_CTRL |= COUNTER_ENABLE; // enable SysTick counter
}

void SysTick_Handler(void)
{
    /* Executed on each 1ms interrupt */
    timer++; // increment timer
}

int main(void)
{
    UART4_GPIO_Init(); // initialize UART4 pin
    UART_Setup(); // setup UART
    SysTick_Init(); // initialize SysTick
    Mic_Setup(); // setup mic
    DFSDM_Setup(); // digital filter setup
    uint8_t setting = 4;

    volatile uint16_t delay;

    while (1)
    {
        Get_Mic_Sample();
        Convert_Uint_To_Bytes(timer, setting); // transmit Timer on D1, PA0
        for (delay = 0; delay < 5000; delay++);
    }
    return 0; // never reached
}