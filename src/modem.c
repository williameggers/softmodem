#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include "sm.h"

enum modem_type {
    MODEM_V22BIS, MODEM_FSK_V21, MODEM_FSK_V23, MODEM_V27TER, MODEM_V17
};
struct {
    enum modem_type type;
    int siglevel;
    int bitrate;
    int freq;
    int guard;
    int framemode;
    int spec[2];
    int tep;
    int oldtrain;
    int shorttrain;
} opt;

struct {
    union {
	void *state;
	struct { void *state_tx, *state_rx; };
    };
    struct {
	async_tx_state_t *state_tx;
	async_rx_state_t *state_rx;
	/* special symbol handling - need to be chosen carefuly ! repetition */
	int	rx_symwas;
	#define SYMBOL_NOTHING 254
	#define SYMBOL_BREAK 252
	int	tx_symneed;
	#define SYMBOL_NEED_CONFIRM 153
	#define SYMBOL_NEED_AVOID 189
	/* error handling */
	int parity_errors;
	int framing_errors;
    } as;
    /* buffers */
    queue_state_t *queue_tx;
    #define QUEUE_TX_SIZE 1024
    uint8_t *dembuf;
    int      dembuf_bytepos;
    int      dembuf_len;
    int16_t *modframes;
    int16_t *demframes;
    /* status */
    float qam_smoothpower;
    int   qam_symbolno;
} m;

static int  cb_get_byte(void *);
static void convert_ui16tof(float *, int16_t *, int);
static void cb_put_byte(void *, int);
static void convert_ftoui16(int16_t *, float *, int);
static void cb_status(void *, int);
static void cb_qam_report(void *, const complexf_t *, const complexf_t *, int);
static void restart();

#define OPT_INT(idx, var, def, test)    \
var = def;                              \
if (sm.modem[idx] != NULL) {            \
    var = atoi(sm.modem[idx]);          \
    if (!test) fatal("invalid %s", #var); }
#define OPT_STR(idx, var, def)          \
var = def;                              \
if (sm.modem[idx] != NULL) {
#define OPT_STR_I(idx, var, str, const) \
if (!strcmp(sm.modem[idx], str)) {      \
    var = const;
#define OPT_STR_E(idx, var, str, const) \
} else OPT_STR_I(idx, var, str, const)
#define OPT_STR_INTEND(idx, var, test)  \
} else var = atoi(sm.modem[idx]);       \
if (!test) fatal("invalid %s", #var); }
#define OPT_STR_END(idx, var)           \
} else fatal("invalid %s", #var); }

void
modem_init()
{
    if (!sm.modem[0]) sm.modem[0] = "v22bis";
    OPT_STR(0, opt.type, MODEM_V22BIS)
	OPT_STR_I(0, opt.type, "v22bis", MODEM_V22BIS)
	    OPT_INT(1, opt.siglevel, 0, (opt.siglevel < 0))
	    OPT_INT(2, opt.bitrate, 1200,
		(opt.bitrate == 1200 || opt.bitrate == 2400))
	    OPT_INT(3, opt.freq, 0, (opt.freq > 0))
	    OPT_STR(4, opt.guard, V22BIS_GUARD_TONE_1800HZ)
		OPT_STR_I(4, opt.guard, "none", V22BIS_GUARD_TONE_NONE)
		OPT_STR_E(4, opt.guard, "550", V22BIS_GUARD_TONE_550HZ)
		OPT_STR_E(4, opt.guard, "1800", V22BIS_GUARD_TONE_1800HZ)
	    OPT_STR_END(4, opt.guard)
	OPT_STR_E(0, opt.type, "fsk_v21", MODEM_FSK_V21)
	    opt.spec[0] = (!sm.caller) ? FSK_V21CH1 : FSK_V21CH2;
	    opt.spec[1] = (!sm.caller) ? FSK_V21CH2 : FSK_V21CH1;
	    OPT_INT(1, opt.siglevel, 0, (opt.siglevel < 0))
	    OPT_STR(2, opt.framemode, FSK_FRAME_MODE_SYNC)
		OPT_STR_I(2, opt.framemode, "async", FSK_FRAME_MODE_ASYNC)
		OPT_STR_E(2, opt.framemode, "sync", FSK_FRAME_MODE_SYNC)
	    OPT_STR_INTEND(2, opt.framemode, (opt.framemode != 0))
	OPT_STR_E(0, opt.type, "fsk_v23", MODEM_FSK_V23)
	    opt.spec[0] = (!sm.caller) ? FSK_V23CH1 : FSK_V23CH2;
	    opt.spec[1] = (!sm.caller) ? FSK_V23CH2 : FSK_V23CH1;
	    OPT_INT(1, opt.siglevel, 0, (opt.siglevel < 0))
	    OPT_STR(2, opt.framemode, FSK_FRAME_MODE_SYNC)
		OPT_STR_I(2, opt.framemode, "async", FSK_FRAME_MODE_ASYNC)
		OPT_STR_E(2, opt.framemode, "sync", FSK_FRAME_MODE_SYNC)
	    OPT_STR_INTEND(2, opt.framemode, (opt.framemode != 0))
	OPT_STR_E(0, opt.type, "v27ter", MODEM_V27TER)
	    OPT_INT(1, opt.siglevel, 0, (opt.siglevel < 0))
	    OPT_INT(2, opt.bitrate, 2400,
		    (opt.bitrate == 2400 || opt.bitrate == 4800))
	    OPT_INT(3, opt.tep, 0, (opt.tep == 0 || opt.tep == 1))
	    OPT_INT(4, opt.oldtrain, 0, (opt.oldtrain == 0 || opt.oldtrain == 1))
	OPT_STR_E(0, opt.type, "v17", MODEM_V17)
	    OPT_INT(1, opt.siglevel, 0, (opt.siglevel < 0))
	    OPT_INT(2, opt.bitrate, 7200,
		    (opt.bitrate == 7200 || opt.bitrate == 9600 ||
		     opt.bitrate == 12000 || opt.bitrate == 14400))
	    OPT_INT(3, opt.tep, 0, (opt.tep == 0 || opt.tep == 1))
	    OPT_INT(4, opt.shorttrain, 0,
		    (opt.shorttrain == 0 || opt.shorttrain == 1))
    OPT_STR_END(0, opt.type)

    m.queue_tx = queue_init(NULL, QUEUE_TX_SIZE,
			    QUEUE_READ_ATOMIC | QUEUE_WRITE_ATOMIC);
    m.modframes = malloc(MODEM_SRATE * sizeof(int16_t));
    m.demframes = malloc(MODEM_SRATE * sizeof(int16_t));
    m.dembuf_len = 1;
    m.dembuf = malloc(m.dembuf_len * sizeof(uint8_t));
    m.as.state_tx = async_tx_init(NULL, 8, ASYNC_PARITY_EVEN, 1, FALSE,
				    cb_get_byte, NULL);
    m.as.state_rx = async_rx_init(NULL, 8, ASYNC_PARITY_EVEN, 1, FALSE,
				    cb_put_byte, NULL);
    m.as.tx_symneed = -1;
    m.as.rx_symwas = -1;
    m.as.parity_errors = 0;
    m.as.framing_errors = 0;

    switch (opt.type) {
	case MODEM_V22BIS:
	    m.state = v22bis_init(NULL, opt.bitrate, opt.guard, (sm.caller == 0),
			async_tx_get_bit, m.as.state_tx,
			async_rx_put_bit, m.as.state_rx);
	    if (opt.siglevel)
		v22bis_tx_power(m.state, opt.siglevel);
	    v22bis_set_modem_status_handler(m.state, cb_status, NULL);
	    v22bis_rx_set_qam_report_handler(m.state, cb_qam_report, NULL);
	    if (opt.freq)
		((v22bis_state_t *)m.state)->tx.carrier_phase_rate =
		    dds_phase_ratef((sm.caller == 0) ? opt.freq : opt.freq+1200);
	    break;
	case MODEM_FSK_V21:
	case MODEM_FSK_V23:
	    m.state_tx = fsk_tx_init(NULL, &preset_fsk_specs[opt.spec[0]],
			async_tx_get_bit, m.as.state_tx);
	    fsk_tx_set_modem_status_handler(m.state_tx, cb_status, NULL);
	    if (opt.siglevel)
		fsk_tx_power(m.state_tx, opt.siglevel);
	    m.state_rx = fsk_rx_init(NULL, &preset_fsk_specs[opt.spec[1]],
			opt.framemode, async_rx_put_bit, m.as.state_rx);
	    fsk_rx_set_modem_status_handler(m.state_rx, cb_status, NULL);
	    break;
	case MODEM_V27TER:
	    m.state_tx = v27ter_tx_init(NULL, opt.bitrate, opt.tep,
			async_tx_get_bit, m.as.state_tx);
	    v27ter_tx_set_modem_status_handler(m.state_tx, cb_status, NULL);
	    if (opt.siglevel)
		v27ter_tx_power(m.state_tx, opt.siglevel);
	    m.state_rx = v27ter_rx_init(NULL, opt.bitrate,
			async_rx_put_bit, m.as.state_rx);
	    v27ter_rx_set_modem_status_handler(m.state_rx, cb_status, NULL);
	    v27ter_rx_set_qam_report_handler(m.state_rx, cb_qam_report, NULL);
	    break;
	case MODEM_V17:
	    m.state_tx = v17_tx_init(NULL, opt.bitrate, opt.tep,
			async_tx_get_bit, m.as.state_tx);
	    v17_tx_set_modem_status_handler(m.state_tx, cb_status, NULL);
	    if (opt.siglevel)
		v17_tx_power(m.state_tx, opt.siglevel);
	    m.state_rx = v17_rx_init(NULL, opt.bitrate,
			async_rx_put_bit, m.as.state_rx);
	    v17_rx_set_modem_status_handler(m.state_rx, cb_status, NULL);
	    v17_rx_set_qam_report_handler(m.state_rx, cb_qam_report, NULL);
	    break;
    }

    m.qam_smoothpower = 0.0f;
    m.qam_symbolno = 0;
}

void
modem_modulate(jack_sample *frames, int nframes)
{
    int done = 0;

    if (sm.sigbreak_local) {
	log_info("modulate: received signal to break, flushing send queue");
	queue_flush(m.queue_tx);
	sm.sigbreak_local = 0;
    }
    if (sm.sigrestart) {
	log_info("modulate: received signal to restart, restarting modem");
	restart();
	sm.sigrestart = 0;
    }

    switch (opt.type) {
	case MODEM_V22BIS:
	    log_debug("modulate: go ! (training %d %d birate %d)",
		((v22bis_state_t *)m.state)->tx.training,
		((v22bis_state_t *)m.state)->tx.training_count,
		v22bis_current_bit_rate(m.state));
	    done = v22bis_tx(m.state, m.modframes, nframes);
	    break;
	case MODEM_FSK_V21:
	case MODEM_FSK_V23:
	    done = fsk_tx(m.state_tx, m.modframes, nframes);
	    break;
	case MODEM_V27TER:
	    done = v27ter_tx(m.state_tx, m.modframes, nframes);
	    break;
	case MODEM_V17:
	    done = v17_tx(m.state_tx, m.modframes, nframes);
	    break;
    }
    log_debug("modulate: %d done", done);
    if (done == 0) {
	log_warn("modulate: nothing done ! restarting modem");
	restart();
    }

    if (sm.use_gui)
	gui_update_audio(m.modframes, done);
    convert_ui16tof(frames, m.modframes, nframes);
}

void
modem_demodulate(jack_sample *frames, int nframes, uint8_t **data, int *datalen)
{
    int len;
    int notdone = 0;

    convert_ftoui16(m.demframes, frames, nframes);
    len = ((nframes * opt.bitrate / MODEM_SRATE / 8) + 1); /* +1 for last byte */
    if (len != m.dembuf_len) {
	log_debug("demodulate: realloc dembuf, %d -> %d", m.dembuf_len, len);
	m.dembuf_len = len;
	m.dembuf = realloc(m.dembuf, len * sizeof(uint8_t));
	if (!m.dembuf)
	    fatal("could not realloc demodulation buffer");
    }
    m.dembuf_bytepos = 0;

    switch (opt.type) {
	case MODEM_V22BIS:
	    log_debug("demodulate: go ! (training %d %d) (power %d)",
		((v22bis_state_t *)m.state)->rx.training,
		((v22bis_state_t *)m.state)->rx.training_count,
		power_meter_current(&((v22bis_state_t *)m.state)->rx.rx_power));
	    notdone = v22bis_rx(m.state, m.demframes, nframes);
	    break;
	case MODEM_FSK_V21:
	case MODEM_FSK_V23:
	    log_debug("demodulate: go ! (power %d)",
		power_meter_current(&((fsk_rx_state_t *)m.state_rx)->power));
	    notdone = fsk_rx(m.state_rx, m.demframes, nframes);
	    break;
	case MODEM_V27TER:
	    log_debug("demodulate: go ! (carrier %d) (power %d)",
		v27ter_rx_carrier_frequency(m.state_rx),
		v27ter_rx_signal_power(m.state_rx));
	    notdone = v27ter_rx(m.state_rx, m.demframes, nframes);
	    break;
	case MODEM_V17:
	    log_debug("demodulate: go ! (carrier %d) (power %d)",
		v17_rx_carrier_frequency(m.state_rx),
		v17_rx_signal_power(m.state_rx));
	    notdone = v17_rx(m.state_rx, m.demframes, nframes);
	    break;
    }
    log_debug("demodulate: %d not done, parity err %d, framing err %d",
	notdone, m.as.state_rx->parity_errors, m.as.state_rx->framing_errors);
    if (notdone)
	log_warn("demodulate: some things not done (%d) !", notdone);

    *datalen = m.dembuf_bytepos;
    *data = m.dembuf;
}

void
modem_modqueue_append(uint8_t *buf, int len)
{
    int written;

    log_debug("modem_modqueue_append: %d bytes", len);
    written = queue_write(m.queue_tx, buf, len);
    if (written != len)
	log_warn("modem_modqueue_append: wanted to write %d, but %d written !",
		len, written);
}

int
modem_modqueue_isready()
{
    int free;

    free = queue_free_space(m.queue_tx);
    log_debug("modem modqueue_isready, queue %d free", free);

    return free;
}

static int
cb_get_byte(void *data)
{
    int b;

    if (m.as.tx_symneed != -1) {
	b = m.as.tx_symneed;
	m.as.tx_symneed = -1;
	if (sm.sigbreak_send == 2)
	    sm.sigbreak_send = 0;
    } else if (sm.sigbreak_send == 1) {
	b = SYMBOL_BREAK;
	m.as.tx_symneed = SYMBOL_NEED_CONFIRM;
	sm.sigbreak_send++;
    } else {
	b = queue_read_byte(m.queue_tx);
	if (b < 0) {
	    b = SYMBOL_NOTHING;
	    m.as.tx_symneed = SYMBOL_NEED_CONFIRM;
	} else if (b == SYMBOL_NOTHING || b == SYMBOL_BREAK) {
	    m.as.tx_symneed = SYMBOL_NEED_AVOID;
	} else
	    m.as.tx_symneed = -1;
    }
    log_debug("modem cb_get_byte %d", b);

    return b;
}

static void
convert_ui16tof(float *f, int16_t *u, int n)
{
    int i;

    for(i=0; i<n; i++) {
	//XXX correct conversion needed
	f[i] = (float)u[i] / 10000;
/*	log_debug("%d -> %f ", u[i], f[i]); */
    }
}

static void
cb_put_byte(void *data, int b)
{
    log_debug("put_byte: parity err %d, framing err %d",
	m.as.state_rx->parity_errors, m.as.state_rx->framing_errors);
    if (m.as.parity_errors != m.as.state_rx->parity_errors ||
	m.as.framing_errors != m.as.state_rx->framing_errors) {
	m.as.parity_errors = m.as.state_rx->parity_errors;
	m.as.framing_errors = m.as.state_rx->framing_errors;
	return;
    }

    log_debug("modem cb_put_byte received %d", b);
    if (m.as.rx_symwas != -1) {
	if (b == SYMBOL_NEED_CONFIRM ||
	    (b == SYMBOL_NOTHING)) { /* missed confirm/avoid, pretend confirm */
	    if (m.as.rx_symwas == SYMBOL_BREAK)
		sm.sigbreak_local = 1;
	    m.as.rx_symwas = -1;
	    return;
	} else if (b == SYMBOL_NEED_AVOID) {
	    b = m.as.rx_symwas;
	    m.as.rx_symwas = -1;
	} else {
	    /* we must have missed one confirm/avoid, drop symbol */
	    m.as.rx_symwas = -1;
	} 
    } else if (b == SYMBOL_NOTHING || b == SYMBOL_BREAK) {
	m.as.rx_symwas = b;
	return;
    }
    log_debug("modem cb_put_byte put %d", b);

    m.dembuf[m.dembuf_bytepos] = b;
    m.dembuf_bytepos++;
}

static void
convert_ftoui16(int16_t *u, float *f, int n)
{
    int i;

    for(i=0; i<n; i++) {
	//XXX correct conversion needed
	if ((f[i] > 0.0 && f[i] < 1.0) ||
	    (f[i] < 0.0 && f[i] > -1.0))
	    u[i] = f[i] * 10000;
	else
	    u[i] = lfastrintf(f[i]);
/*	log_debug("%f -> %d ", f[i], u[i]); */
    }
}

static void
cb_status(void *data, int status)
{
    log_info("modem cb_status: %s (%d)", signal_status_to_str(status), status);
    if (opt.type == MODEM_V22BIS)
	if (status == SIG_STATUS_TRAINING_SUCCEEDED)
	    log_info("Negociated %d bps", v22bis_current_bit_rate(m.state));
}

static void
cb_qam_report(void *data,
    const complexf_t *constel, const complexf_t *target, int symbol)
{
    int i;
    int len;
    complexf_t *coeffs;
    float fpower;

    switch (opt.type) {
	case MODEM_V22BIS:
	if (constel)
	{
	    if (sm.use_gui)
		gui_update_constel(constel, v22bis_rx_carrier_frequency(m.state),
				    v22bis_rx_symbol_timing_correction(m.state));

	    fpower = (constel->re - target->re)*(constel->re - target->re)
		   + (constel->im - target->im)*(constel->im - target->im);
	    m.qam_smoothpower = 0.95f*m.qam_smoothpower + 0.05f*fpower;
/*
	    log_debug("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %2x %8.4f %8.4f %8.4f",
		   m.qam_symbolno, constel->re, constel->im, target->re, target->im,
		   symbol, fpower, m.qam_smoothpower,
		   v22bis_rx_signal_power(m.state)); */
	    m.qam_symbolno++;
	}
	else
	{
/*	    log_debug("Gardner step %d", symbol); */
	    len = v22bis_rx_equalizer_state(m.state, &coeffs);
/*	    log_debug("Equalizer A:"); */
	    for (i = 0;  i < len;  i++)
/*		log_debug("%3d (%15.5f, %15.5f) -> %15.5f",
			i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i])); */
	    if (sm.use_gui)
		gui_update_eq(coeffs, len);
	}
	break;
	case MODEM_FSK_V21:
	case MODEM_FSK_V23:
	/* NOT APPLICABLE */
	case MODEM_V27TER:
	case MODEM_V17:
	/* NOT IMPLEMENTED */
	    break;
    }
}

static void
restart(void)
{
    switch (opt.type) {
	case MODEM_V22BIS:
	    v22bis_restart(m.state, opt.bitrate);
	    break;
	case MODEM_FSK_V21:
	case MODEM_FSK_V23:
	    fsk_tx_restart(m.state_tx, &preset_fsk_specs[opt.spec[0]]);
	    fsk_rx_restart(m.state_rx, &preset_fsk_specs[opt.spec[1]], opt.framemode);
	    break;
	case MODEM_V27TER:
	    v27ter_tx_restart(m.state_tx, opt.bitrate, opt.tep);
	    v27ter_rx_restart(m.state_rx, opt.bitrate, opt.oldtrain);
	    break;
	case MODEM_V17:
	    v17_tx_restart(m.state_tx, opt.bitrate, opt.tep, opt.shorttrain);
	    v17_rx_restart(m.state_rx, opt.bitrate, opt.shorttrain);
	    break;
    }
}
