#include <FreeRTOS.h>
#include <AT91SAM7.h>
#include <board.h>
#include <beacontypes.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>

#include <task.h>

#include "led.h"
#include "power.h"
#include "accelerometer.h"
#include "eink/eink.h"
#include "eink/eink_flash.h"
#include "ebook/event.h"

#include "image/splash.h"

extern const struct splash_image bg_splash_image;

#define DISPLAY_SHORT (EINK_CURRENT_DISPLAY_CONFIGURATION->vsize)
#define DISPLAY_LONG (EINK_CURRENT_DISPLAY_CONFIGURATION->hsize)
#define ROUND_UP(i,j) ((j)*(((i)+(j-1))/(j)))

/* Warning: The actual display size must not exceed this definition. E.g.
 * assert( IMAGE_SIZE >= EINK_CURRENT_DISPLAY_CONFIGURATION->hsize * EINK_CURRENT_DISPLAY_CONFIGURATION->vsize );
 */
#define IMAGE_SIZE (1216*832)

static uint8_t eink_mgmt_data[10240] __attribute__ ((section (".sdram")));
static eink_image_buffer_t blank_buffer, black_buffer, bg_buffer;
static uint8_t __attribute__((aligned(32), section (".sdram")))
	_blank_data[IMAGE_SIZE], _black_data[IMAGE_SIZE], _bg_data[IMAGE_SIZE];
static struct image blank_image = {
		.data = _blank_data,
		.size = sizeof(_blank_data),
}, black_image = {
		.data = _black_data,
		.size = sizeof(_black_data),
}, bg_image = {
		.data = _bg_data,
		.size = sizeof(_bg_data),
};

static enum eink_pack_mode bpp_to_pack_mode(enum image_bpp in) {
	switch(in) {
	case IMAGE_BPP_2: return PACK_MODE_2BIT;
	case IMAGE_BPP_4: return PACK_MODE_4BIT;
	case IMAGE_BPP_8: return PACK_MODE_1BYTE;
	}
	return 0;
}



static void clear_screen(void)
{
	eink_job_t job;
	eink_job_begin(&job, 0);
	eink_job_add(job, blank_buffer, WAVEFORM_MODE_GC, UPDATE_MODE_FULL);
	eink_job_commit(job);
}

static void paint_die_error(enum eink_error error)
{
	switch(error) {
	case EINK_ERROR_NOT_DETECTED:
		printf("eInk controller not detected\n"); break;
	case EINK_ERROR_NOT_SUPPORTED:
		printf("eInk controller not supported (unknown revision)\n"); break;
	case EINK_ERROR_COMMUNICATIONS_FAILURE:
		printf("eInk controller communications failure, check FPC cable and connector\n"); break;
	case EINK_ERROR_CHECKSUM_ERROR:
		printf("eInk controller waveform flash checksum failure\n"); break;
	default:
		printf("eInk controller initialization: unknown error\n"); break;
	}
	led_halt_blinking(2);
}

static int event_loop_running = 0;
static void paint_task(void *params)
{
	(void)params;
	int error, idle_time=0;
	vTaskDelay(100/portTICK_RATE_MS);
	
	/*
	 * bg_splash_image is in the flash
	 * black_data, blank_data, bg_data is in the microcontroller SDRAM
	 * black_buffer, blank_buffer, bg_buffer are in the display controller SDRAM
	 */
	
	error = image_unpack_splash(&bg_image, &bg_splash_image);
	if(error < 0) {
		printf("Error during splash unpack: %i (%s)\n", error, strerror(-error));
		led_halt_blinking(3);
	}
	
	blank_image.bits_per_pixel = IMAGE_BPP_2;
	blank_image.rowstride = ROUND_UP(DISPLAY_SHORT, 8) / 4;
	
	black_image.bits_per_pixel = IMAGE_BPP_2;
	black_image.rowstride = ROUND_UP(DISPLAY_SHORT, 8) / 4;
	
	error = image_create_solid(&blank_image, 0xff, DISPLAY_SHORT, DISPLAY_LONG);
	if(error < 0) {
		printf("Error during blank create: %i (%s)\n", error, strerror(-error));
		led_halt_blinking(3);
	}

	error = image_create_solid(&black_image, 0x00, DISPLAY_SHORT, DISPLAY_LONG);
	if(error < 0) {
		printf("Error during black create: %i (%s)\n", error, strerror(-error));
		led_halt_blinking(3);
	}
	
	vTaskDelay(5000/portTICK_RATE_MS);

	eink_mgmt_init(eink_mgmt_data, sizeof(eink_mgmt_data));

#if 0 /* FIXME Seems to be broken, sometimes (hangs at boot) */
	error = eink_flash_conditional_reflash();
	if(error > 0) { /* Not an error, but the controller was reinitialized and we must wait */
		vTaskDelay(5/portTICK_RATE_MS);
	} else if(error < 0) {
		paint_die_error(-error);
	}
#endif

	eink_controller_reset();

	printf("Initializing eInk controller ...\n");
	error=eink_controller_init();
	if(error < 0) paint_die_error(-error);
	printf("OK\n");
	
	if( (error=eink_image_buffer_acquire(&blank_buffer)) < 0) {
		printf("Reason: %i\nCould not acquire blank image buffer\n", error);
		led_halt_blinking(3);
	}
	if( (error=eink_image_buffer_acquire(&black_buffer)) < 0) {
		printf("Reason: %i\nCould not acquire black image buffer\n", error);
		led_halt_blinking(3);
	}
	if( (error=eink_image_buffer_acquire(&bg_buffer)) < 0) {
		printf("Reason: %i\nCould not acquire bg image buffer\n", error);
		led_halt_blinking(3);
	}
	
	error = 0;
	
	portTickType start = xTaskGetTickCount(), stop, cumulative=0;
	error |= eink_image_buffer_load(blank_buffer, bpp_to_pack_mode(blank_image.bits_per_pixel), ROTATION_MODE_90,
			blank_image.data, blank_image.rowstride*blank_image.height) ;
	stop = xTaskGetTickCount();
	printf("Blank image: %li\n", (long)(stop-start));
	cumulative += stop-start;
	
	start = xTaskGetTickCount();
	error |= eink_image_buffer_load(black_buffer, bpp_to_pack_mode(black_image.bits_per_pixel), ROTATION_MODE_90,
			black_image.data, black_image.rowstride*black_image.height);
	stop = xTaskGetTickCount();
	printf("Black image: %li\n", (long)(stop-start));
	cumulative += stop-start;
	
	start = xTaskGetTickCount();
	error |= eink_image_buffer_load(bg_buffer, bpp_to_pack_mode(blank_image.bits_per_pixel), ROTATION_MODE_90,
			blank_image.data, blank_image.rowstride*blank_image.height);
	stop = xTaskGetTickCount();
	printf("Blank image: %li\n", (long)(stop-start));
	cumulative += stop-start;
	
	start = xTaskGetTickCount();
	error |= eink_image_buffer_load_area(bg_buffer, bpp_to_pack_mode(bg_image.bits_per_pixel), ROTATION_MODE_90,
			(DISPLAY_SHORT-bg_image.width)/2, (DISPLAY_LONG-bg_image.height)/2,
			bg_image.width, bg_image.height,
			bg_image.data, bg_image.size);
	stop = xTaskGetTickCount();
	printf("Background image: %li\n", (long)(stop-start));
	cumulative += stop-start;
	
	printf("All images loaded in %li ticks\n", (long)(cumulative));

	if(error >= 0) {
		printf("All images loaded ok\n");
		eink_job_t job;
		eink_job_begin(&job, 0);
		eink_job_add(job, blank_buffer, WAVEFORM_MODE_INIT, UPDATE_MODE_INIT);
		eink_job_commit(job);
	} else {
		printf("Image load error %i: %s\n", error, strerror(-error));
	}

	int i=0;
	eink_job_t job;
	eink_job_begin(&job, 0);
	eink_job_add(job, bg_buffer, WAVEFORM_MODE_GC, UPDATE_MODE_FULL);
	eink_job_commit(job);

	event_t received_event;
	event_loop_running = 1;
	int battery_update_counter = 5;
#define WAIT_TIME 25
#define MAX_IDLE ((10 * 60 * 1000) / WAIT_TIME)
#define BATTERY_UPDATE ((2 * 1000) / WAIT_TIME)
	while(1) {
		received_event.class = EVENT_NONE;
		event_receive(&received_event, WAIT_TIME/portTICK_RATE_MS);
		i++;

		switch(received_event.class) {
		case EVENT_POWEROFF:
			clear_screen();
			while(eink_job_count_pending() > 0) vTaskDelay(10/portTICK_RATE_MS);
			power_off();
		default:
			break;
		}

		if(received_event.class == EVENT_NONE && !power_is_usb_connected()) {
			idle_time++;
		} else {
			idle_time=0;
		}
		if(idle_time > MAX_IDLE) {
			clear_screen();
			while(eink_job_count_pending() > 0) vTaskDelay(10/portTICK_RATE_MS);
			power_off();
		}
		
		if(battery_update_counter++ >= BATTERY_UPDATE) {
			const int MIN_V = 600, MAX_V = 900;
			int voltage = power_get_battery_voltage();
			battery_update_counter = 0;
			int barlen = (DISPLAY_LONG * (voltage-MIN_V)) / (MAX_V-MIN_V);
			if(barlen >= DISPLAY_LONG) barlen = DISPLAY_LONG-1;
			if(barlen < 0) barlen = 0;
			
			eink_job_t job;
			eink_job_begin(&job, 0);
			if(barlen > 0) {
				eink_job_add_area(job, blank_buffer, WAVEFORM_MODE_GU, UPDATE_MODE_FULL, 
						DISPLAY_SHORT-1, 0, 
						1, barlen);
			}
			if(barlen < DISPLAY_LONG-1) {
				eink_job_add_area(job, black_buffer, WAVEFORM_MODE_GU, UPDATE_MODE_FULL, 
						DISPLAY_SHORT-1, DISPLAY_LONG-1 - barlen, /* x, y */ 
						1, DISPLAY_LONG-1); /* width, height */
			}
			eink_job_commit(job);
		}
	}
}

static void power_pressed_cb(void)
{
	if(event_loop_running)
		event_send(EVENT_POWEROFF, 0);
	else
		power_off();
}


int paint_init(void)
{
	int r;
	if( (r=event_init()) < 0)
		return r;

	if( (r=power_set_pressed_callback(power_pressed_cb)) < 0)
		return r;

	xTaskCreate(paint_task, (signed portCHAR *) "PAINT_ TASK", TASK_EBOOK_STACK,
			NULL, TASK_EBOOK_PRIORITY, NULL);

	return 0;
}
