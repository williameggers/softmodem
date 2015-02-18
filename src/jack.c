#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

#include "sm.h"

jack_client_t *client;
jack_port_t *input_port;
jack_port_t *output_port;

static int  cb_jack_process(jack_nframes_t, void *);
static int  cb_jack_srate(jack_nframes_t, void *);
static void cb_jack_error(const char *);
static void cb_jack_shutdown(void *);

void
jack_init(int connect_ports)
{
    const char **ports;
    int i;

    jack_set_error_function(cb_jack_error);
    if (!(client = jack_client_open(sm.name,
				    JackNoStartServer|JackUseExactName, NULL)))
	fatal("jack server not running");
    jack_set_process_callback(client, cb_jack_process, NULL);
    jack_set_sample_rate_callback(client, cb_jack_srate, NULL);
    jack_on_shutdown(client, cb_jack_shutdown, NULL);
    input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE,
				    JackPortIsInput, 0);
    output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE,
				    JackPortIsOutput, 0);
    if (jack_activate(client))
	fatal("cannot activate jack client");
    
    if (connect_ports) {
	ports = jack_get_ports(client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (!ports)
	    fatal("cannot find physical playback port");
	for(i=0; ports[i] != NULL; i++)
	    if (jack_connect(client, jack_port_name(output_port), ports[i]))
		log_warn("cannot connect output port to %s", ports[i]);
	free(ports);
	ports = jack_get_ports(client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (!ports)
	    fatal("cannot find physical input port");
	for(i=0; ports[i] != NULL; i++)
	    if (jack_connect(client, ports[i], jack_port_name(input_port)))
		log_warn("cannot connect input port %s", ports[i]);
	free(ports);
    }
}

static int
cb_jack_process(jack_nframes_t nframes, void *arg)
{
    jack_sample *buf;
    uint8_t *data;
    int datalen;

    if (!sm.on) return 0;

    /* output */
    buf = (jack_sample *)jack_port_get_buffer(output_port, nframes);
    modem_modulate(buf, nframes);

    /* input */
    buf = (jack_sample *)jack_port_get_buffer(input_port, nframes);
    modem_demodulate(buf, nframes, &data, &datalen);
    if (datalen > 0)
	tty_write(data, datalen);

    return 0;
}

static int
cb_jack_srate(jack_nframes_t nframes, void *arg)
{
    if (nframes != MODEM_SRATE)
	fatal("jack cb_srate: sample rate must be %d", MODEM_SRATE);
    return 0;
}

static void
cb_jack_error(const char *desc)
{
    log_warn("jack error %s", desc);
}

static void
cb_jack_shutdown(void *arg)
{
    fatal("jack cb_jack_shutdown called, exiting");
}

