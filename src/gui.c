#include "sm.h"

#ifdef ENABLE_GUI
#include "gui/modem_monitor.h"
qam_monitor_t *qam_mon;
#endif

void
gui_init()
{
#ifdef ENABLE_GUI
    qam_mon = qam_monitor_init(6.0f, sm.name);
    if (!qam_mon)
	fatal("failed to initialize qam monitor");
#else
    log_info("gui_init: failed to use the gui, it is not compiled in !");
#endif
}

void
gui_update_audio(int16_t *frames, int len)
{
#ifdef ENABLE_GUI
    qam_monitor_update_audio_level(qam_mon, frames, len);
#endif
}

void
gui_update_constel(const complexf_t *constel, float carrier, float correction)
{
#ifdef ENABLE_GUI
    qam_monitor_update_constel(qam_mon, constel);
    qam_monitor_update_carrier_tracking(qam_mon, carrier);
    qam_monitor_update_symbol_tracking(qam_mon, correction);
#endif
}

void
gui_update_eq(const complexf_t *coeffs, int len)
{
#ifdef ENABLE_GUI
    qam_monitor_update_equalizer(qam_mon, coeffs, len);
#endif
}
