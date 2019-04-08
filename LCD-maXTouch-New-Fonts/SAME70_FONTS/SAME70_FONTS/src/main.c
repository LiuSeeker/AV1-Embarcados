/*
* main.c
*
* Created: 05/03/2019 18:00:58
*  Author: eduardo
*/

/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"

/************************************************************************/
/* defines                                                              */
/************************************************************************/

#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX  19
#define BUT_IDX_MASK (1 << BUT_IDX)

#define BUT2_PIO      PIOC
#define BUT2_PIO_ID   ID_PIOC
#define BUT2_IDX  31
#define BUT2_IDX_MASK (1 << BUT2_IDX)

#define LED_PIO           PIOC
#define LED_PIO_ID        12
#define LED_IDX       8u
#define LED_IDX_MASK  (1u << LED_IDX)

/************************************************************************/
/* constants                                                            */
/************************************************************************/

struct ili9488_opt_t g_ili9488_display_opt;

const float raio = 0.325;

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

volatile Bool rtt_flag = true;
volatile Bool rtc_flag = true;
volatile Bool led_flag = false;
volatile Bool stop_flag = false;

volatile int dhora_count = 0;
volatile int hora_count = 0;
volatile int dmin_count = 0;
volatile int min_count = 0;
volatile int dsec_count = 0;
volatile int sec_count = 0;

volatile int counter = 0;
volatile float distancia = 0;

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

void pisca_led();
void io_init(void);
void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);
void font_draw_text(tFont *font, const char *text, int x, int y, int spacing);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

void BUT_Handler(void){
	led_flag = true;
}

void BUT2_Handler(void){
	pisca_led();
	if(!stop_flag){
		rtc_disable_interrupt(RTC,RTC_IER_ALREN);
		rtt_disable_interrupt(RTT,RTT_MR_ALMIEN);
		pio_disable_interrupt(BUT_PIO, BUT_IDX_MASK);
		stop_flag = true;
	}
	else{
		rtc_enable_interrupt(RTC,RTC_IER_ALREN);
		rtt_enable_interrupt(RTT,RTT_MR_ALMIEN);
		pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);
		stop_flag = false;
	}
}


void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		rtt_flag = true;                  // flag RTT alarme
	}
}

void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);

	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
		
		int ano, mes, dia, sem, hora, min, sec;
		rtc_get_date(RTC, &ano, &mes, &dia, &sem);
		rtc_get_time(RTC, &hora, &min, &sec);
		
		rtc_set_date_alarm(RTC, 1, mes, 1, dia);
		if (sec >= 59){
			rtc_set_time_alarm(RTC, 1, hora, 1, min+1, 1, sec-59);
		}
		else{
			rtc_set_time_alarm(RTC, 1, hora, 1, min, 1, sec+1);
		}
	}
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
	
	rtc_flag = true;
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
	
}

void clear_lcd_top(void){
	float velocidade = 0;
	char buffer1[32];
	char buffer2[32];
	float dist = raio * 2 * 3.1415 * counter;
	distancia += dist;
	velocidade = dist / 4;
	
	counter = 0;
	
	ili9488_draw_filled_rectangle(0, 40, ILI9488_LCD_WIDTH-1, 80);
	sprintf(buffer1, "Velocidade: %f", velocidade);
	font_draw_text(&calibri_36, buffer1, 40, 80, 1);
	
	ili9488_draw_filled_rectangle(0, 120, ILI9488_LCD_WIDTH-1, 160);
	sprintf(buffer2, "Distancia: %f", distancia);
	font_draw_text(&calibri_36, buffer2, 40, 160, 1);
}

void clear_lcd_t(void){
	char buffer[32];
	ili9488_draw_filled_rectangle(0, 200, ILI9488_LCD_WIDTH-1, 240);
	sprintf(buffer, "Tempo: %d%d:%d%d:%d%d", dhora_count, hora_count, dmin_count, min_count, dsec_count, sec_count);
	font_draw_text(&calibri_36, buffer, 40, 240, 1);
}

void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}
}

void pisca_led(){
	pio_set(LED_PIO,LED_IDX_MASK);
	delay_ms(100);
	pio_clear(LED_PIO,LED_IDX_MASK);
}


void io_init(void){
	/* led */
	pmc_enable_periph_clk(LED_PIO_ID);
	
	
	pio_configure(LED_PIO, PIO_OUTPUT_1, LED_IDX_MASK, PIO_DEFAULT);

}

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	int ano, mes, dia, sem, hora, min, sec;
	rtc_get_date(RTC, &ano, &mes, &dia, &sem);
	rtc_get_time(RTC, &hora, &min, &sec);
	rtc_set_date(RTC, ano, mes, dia, sem);
	rtc_set_time(RTC, hora, min, sec);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(RTC, RTC_IER_ALREN);

}

void BUT_init(void){
	/* config. pino botao em modo de entrada */
	pmc_enable_periph_clk(BUT_PIO_ID);
	pio_set_input(BUT_PIO, BUT_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pio_set_input(BUT2_PIO, BUT2_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);

	pio_configure(BUT_PIO, PIO_INPUT, BUT_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_IDX_MASK, PIO_PULLUP);
	
	pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);
	pio_enable_interrupt(BUT2_PIO, BUT2_IDX_MASK);
	
	/* habilita interrup?c?o do PIO que controla o botao */
	/* e configura sua prioridade                        */
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);
	NVIC_SetPriority(BUT2_PIO_ID, 4);
	
	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_IDX_MASK, PIO_IT_FALL_EDGE, BUT_Handler);
	pio_handler_set(BUT2_PIO, BUT2_PIO_ID, BUT2_IDX_MASK, PIO_IT_FALL_EDGE, BUT2_Handler);
	
	
	
};

void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));
	
	rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}

int main(void) {
	board_init();
	sysclk_init();
	configure_lcd();
	
	font_draw_text(&calibri_36, "Velocidade: 0.0", 40, 80, 1);
	font_draw_text(&calibri_36, "Distancia: 0.0", 40, 160, 1);
	font_draw_text(&calibri_36, "Tempo: 00:00:00", 40, 240, 1);
	
	RTC_init();
	BUT_init();
	io_init();
	
	int ano, mes, dia, sem, hora, min, sec;
	rtc_get_date(RTC, &ano, &mes, &dia, &sem);
	rtc_get_time(RTC, &hora, &min, &sec);
	rtc_set_date_alarm(RTC, 1, mes, 1, dia);
	if(sec >= 59){
		rtc_set_time_alarm(RTC, 1, hora, 1, min+1, 1, sec-59);
	}
	else{
		rtc_set_time_alarm(RTC, 1, hora, 1, min, 1, sec+1);
	}
	
	while(1) {
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
		if(!stop_flag){
		
			if (led_flag){
				pisca_led();
				counter += 1;
				led_flag = false;
			}
		
			if (rtt_flag){
			
			
				clear_lcd_top();
			
			
				uint16_t pllPreScale = (int) (((float) 32768) / 1.0);
				uint32_t irqRTTvalue  = 4;
			
				// reinicia RTT para gerar um novo IRQ
				RTT_init(pllPreScale, irqRTTvalue);
			
				rtt_flag = false;
			}
		
			if (rtc_flag){
			
				if(sec_count < 9){
					sec_count +=1;
				}
				else{
					sec_count = 0;
					if(dsec_count < 6){
						dsec_count +=1;
					}
					else{
						dsec_count = 0;
						if(min_count < 9){
							min_count +=1;
						}
						else{
							min_count = 0;
							if(dmin_count < 6){
								dmin_count +=1;
							}
							else{
								dmin_count = 0;
								if(hora_count < 9){
									hora_count +=1;
								}
								else{
									hora_count = 0;
									dhora_count +=1;
								}
							}
						}
					}
				}
			
				clear_lcd_t();
				rtc_flag = false;
			}
		}
	}
}