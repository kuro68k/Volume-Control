/*
 * hw.h
 *
 */ 


#ifndef HW_H_
#define HW_H_


#define BTN1_PORT		PORTA
#define BTN1_PIN_bm		PIN1_bm
#define BTN2_PORT		PORTC
#define BTN2_PIN_bm		PIN3_bm

#define RCLK_PORT		PORTA
#define RCLK_PIN_bm		PIN0_bm
#define SCK_PORT		PORTD
#define SCK_PIN_bm		PIN6_bm
#define MOSI_PORT		PORTD
#define MOSI_PIN_bm		PIN4_bm

#define ROTA_PORT		PORTD
#define ROTA_VPORT		VPORTD
#define ROTA_PIN_bm		PIN7_bm
#define ROTB_PORT		PORTD
#define ROTB_VPORT		VPORTD
#define ROTB_PIN_bm		PIN5_bm


extern void HW_init(void);


#endif /* HW_H_ */