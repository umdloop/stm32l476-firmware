#ifndef PCB_LED_SYSTEM_H
#define PCB_LED_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Round-robin scheduled controller */
void pcb_led_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif /* PCB_LED_SYSTEM_H */
