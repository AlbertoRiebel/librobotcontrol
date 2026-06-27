/**
 * @file servo.c
 * @brief Servo control via RPMsg for kernel 5.10
 *
 * Replaces shared memory access with RPMsg communication
 * through /dev/rpmsg_pru31.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <rc/pru.h>
#include <rc/gpio.h>
#include <rc/servo.h>
#include <rc/time.h>

#define TOL                          0.01
#define GPIO_POWER_PIN               2,16
#define RPMSG_PRU1                   "/dev/rpmsg_pru31"

static int pru_fd    = -1;
static int init_flag = 0;
static int esc_max_us = RC_ESC_DEFAULT_MAX_US;
static int esc_min_us = RC_ESC_DEFAULT_MIN_US;

int rc_servo_init(void)
{
        if(rc_gpio_init(GPIO_POWER_PIN, GPIOHANDLE_REQUEST_OUTPUT) == -1){
                fprintf(stderr, "ERROR en rc_servo_init: fallo rc_gpio_init\n");
                return -1;
        }

        pru_fd = open(RPMSG_PRU1, O_RDWR);
        if(pru_fd < 0){
                fprintf(stderr, "ERROR: No se pudo abrir %s\n", RPMSG_PRU1);
                rc_gpio_cleanup(GPIO_POWER_PIN);
                return -1;
        }

        init_flag = 1;
        return 0;
}

void rc_servo_cleanup(void)
{
        if(init_flag != 0){
                rc_gpio_set_value(GPIO_POWER_PIN, 0);
                rc_gpio_cleanup(GPIO_POWER_PIN);
        }
        /* Enviar mensaje de reset al PRU antes de cerrar */
        if(pru_fd >= 0){
                uint32_t payload[2] = { 0, 0 };
                write(pru_fd, payload, sizeof(payload));
                usleep(10000);
                close(pru_fd);
                pru_fd = -1;
        }
        usleep(100000);
        init_flag = 0;
}

int rc_servo_power_rail_en(int en)
{
        if(init_flag == 0) return -1;
        return rc_gpio_set_value(GPIO_POWER_PIN, en);
}

int rc_servo_set_esc_range(int min, int max)
{
        if(min < 1 || max < 2 || min >= max) return -1;
        esc_min_us = min;
        esc_max_us = max;
        return 0;
}

int rc_servo_send_pulse_us(int ch, int us)
{
        if(ch < 0 || ch > RC_SERVO_CH_MAX || init_flag == 0) return -1;

        uint32_t num_loops = (uint32_t)((us * 93712.0) / 23440.0);
        uint32_t payload[2] = { (uint32_t)ch, num_loops };

        if(write(pru_fd, payload, sizeof(payload)) < 0) return -1;
        return 0;
}

int rc_servo_send_pulse_normalized(int ch, double input)
{
        if(input < (-1.5 - TOL) || input > (1.5 + TOL)) return -1;
        return rc_servo_send_pulse_us(ch, 1500 + lround(input * 600.0));
}

int rc_servo_send_esc_pulse_normalized(int ch, double input)
{
        if(input < (-0.1 - TOL) || input > (1.0 + TOL)) return -1;
        return rc_servo_send_pulse_us(ch,
                esc_min_us + lround(input * (esc_max_us - esc_min_us)));
}

int rc_servo_send_oneshot_pulse_normalized(int ch, double input)
{
        if(input < (-0.1 - TOL) || input > (1.0 + TOL)) return -1;
        return rc_servo_send_pulse_us(ch, 125 + lround(input * 125.0));
}
