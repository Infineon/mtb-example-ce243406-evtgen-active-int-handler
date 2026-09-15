/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the Event generator active interrupt 
*              handler Example for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cybsp.h"
#include "cy_evtgen.h"
#include "cy_sysint.h"

/******************************************************************************
* Macros
*******************************************************************************/
#define EVTGEN_STRUCT_NUM                (0U)
#define EVTGEN_REF_DIVIDER               (200UL)
/* LED blink period in milliseconds. */
#define EVTGEN_BLINK_PERIOD_MS           (500UL)
#define EVTGEN_CPU_IRQ                    NvicMux6_IRQn

/*******************************************************************************
* Global Variables
*******************************************************************************/
static volatile uint32_t g_evtgen_interval_ticks = 0UL;
static volatile uint32_t g_evtgen_irq_count = 0UL;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static void evtgen_active_isr(void);
static bool init_evtgen_active_interrupt(void);
static void evtgen_schedule_next_from_now(void);

/*******************************************************************************
* Function Definitions
*******************************************************************************/
static void evtgen_active_isr(void)
{
    Cy_EvtGen_ClearStructInterrupt(EVTGEN0, EVTGEN_STRUCT_NUM);

    g_evtgen_irq_count++;
    Cy_GPIO_Inv(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_NUM);

    evtgen_schedule_next_from_now();
}

static void evtgen_schedule_next_from_now(void)
{
    const uint32_t next_compare = Cy_EvtGen_GetCounterValue(EVTGEN0) + g_evtgen_interval_ticks;

    EVTGEN0->COMP_STRUCT[EVTGEN_STRUCT_NUM].COMP_CTL &= ~EVTGEN_COMP_STRUCT_COMP_CTL_COMP0_EN_Msk;
    EVTGEN0->COMP_STRUCT[EVTGEN_STRUCT_NUM].COMP0 = next_compare;
    EVTGEN0->COMP_STRUCT[EVTGEN_STRUCT_NUM].COMP_CTL |= EVTGEN_COMP_STRUCT_COMP_CTL_COMP0_EN_Msk;
}

static bool init_evtgen_active_interrupt(void)
{
    const uint32_t frequency_ref_hz = Cy_SysClk_ClkHfGetFrequency(3U);
    const uint32_t frequency_tick_hz = frequency_ref_hz / EVTGEN_REF_DIVIDER;

    if ((frequency_ref_hz == 0UL) || (frequency_tick_hz == 0UL))
    {
        return false;
    }

    g_evtgen_interval_ticks = (frequency_tick_hz * EVTGEN_BLINK_PERIOD_MS) / 1000UL;
    if (g_evtgen_interval_ticks == 0UL)
    {
        return false;
    }

    cy_stc_evtgen_config_t evtgen_config =
    {
        .frequencyRef = frequency_ref_hz,
        .frequencyLf = Cy_SysClk_ClkLfGetFrequency(),
        .frequencyTick = frequency_tick_hz,
        .ratioControlMode = CY_EVTGEN_RATIO_CONTROL_SW,
        .ratioValueDynamicMode = CY_EVTGEN_RATIO_DYNAMIC_MODE0,
    };

    cy_stc_evtgen_struct_config_t evtgen_struct_config =
    {
        .functionalitySelection = CY_EVTGEN_ACTIVE_FUNCTIONALITY,
        .triggerOutEdge = CY_EVTGEN_EDGE_SENSITIVE,
        .valueActiveComparator = g_evtgen_interval_ticks,
        .valueDeepSleepComparator = 0UL,
    };

    const cy_stc_sysint_t evtgen_irq_cfg =
    {
        .intrSrc = (cy_sysint_int_src_t)((((uint32_t)EVTGEN_CPU_IRQ) << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) |
                                         ((uint32_t)evtgen_0_interrupt_IRQn)),
        .intrPriority = 3U,
    };

    const cy_en_sysint_status_t irq_status = Cy_SysInt_Init(&evtgen_irq_cfg, evtgen_active_isr);
    if (CY_SYSINT_SUCCESS != irq_status)
    {
        return false;
    }
    NVIC_EnableIRQ(EVTGEN_CPU_IRQ);

    if (CY_EVTGEN_SUCCESS != Cy_EvtGen_Init(EVTGEN0, &evtgen_config))
    {
        return false;
    }

    Cy_EvtGen_Enable(EVTGEN0);

    for (uint32_t retry = 0UL; retry < 2000UL; retry++)
    {
        if (CY_EVTGEN_COUNTER_STATUS_VALID == Cy_EvtGen_GetCounterStatus(EVTGEN0))
        {
            break;
        }
        Cy_SysLib_DelayUs(10UL);
        if (retry == 1999UL)
        {
            return false;
        }
    }

    if (CY_EVTGEN_SUCCESS != Cy_EvtGen_InitStruct(EVTGEN0, EVTGEN_STRUCT_NUM, &evtgen_struct_config))
    {
        return false;
    }

    return true;
}

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function for CPU.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    bool evtgen_ok;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    Cy_GPIO_Pin_FastInit(CYBSP_USER_LED1_PORT,
                         CYBSP_USER_LED1_NUM,
                         CY_GPIO_DM_STRONG_IN_OFF,
                         CYBSP_LED_STATE_OFF,
                         HSIOM_SEL_GPIO);

    evtgen_ok = init_evtgen_active_interrupt();

    for (;;)
    {
        if (evtgen_ok)
        {
            __WFI();
        }
        else
        {
            Cy_GPIO_Inv(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_NUM);
            Cy_SysLib_Delay(150UL);
        }
    }
}

/* [] END OF FILE */
