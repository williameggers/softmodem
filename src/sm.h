#include <jack/types.h>
typedef jack_default_audio_sample_t jack_sample;
#include <spandsp.h>
#include <stdarg.h>
#include <signal.h>

#define SM_USER "_sm"
#define MODEM_SRATE 8000

struct {
    char *name;
    int use_gui;
    char *modem[6];
    unsigned int caller;
    
    /* can be changed in realtime */
    volatile sig_atomic_t on;
    volatile sig_atomic_t sigrestart;
    volatile sig_atomic_t sigbreak_local;
    volatile sig_atomic_t sigbreak_send;
} sm;

void jack_init(int);

void tty_init(int);
void tty_write(uint8_t *, int);
void tty_read_loop();

void modem_init();
void modem_modulate(jack_sample *, int);
void modem_demodulate(jack_sample *, int, uint8_t **, int *);
void modem_modqueue_append(uint8_t *, int);
int  modem_modqueue_isready();

void log_init(int);
void log_tmp(const char *, ...);
void log_debug(const char *, ...);
void log_info(const char *, ...);
void log_warn(const char *, ...);
void fatal(const char *, ...);
void log_ttytofile(const char *);

void gui_init(void);
void gui_update_audio(int16_t *, int);
void gui_update_constel(const complexf_t *, float, float);
void gui_update_eq(const complexf_t *coeffs, int);
