#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_CHANNEL	2
#define DEFAULT_MIN	10
#define DEFAULT_MAX	0x7FF
#define DEFAULT_STEP	10
#define DEFAULT_TIMEOUT 200

#define XSTR(s)	STR(s)
#define STR(s)	#s

static const char* options = "c:l:u:s:t:hv";
static const struct option long_options[] = {
	{ "channel",	required_argument,	0,	'c' },
	{ "min",	required_argument,	0,	'l' },
	{ "max",	required_argument,	0,	'u' },
	{ "step",	required_argument,	0,	's' },
	{ "timeout",	required_argument,	0,	't' },
	{ "help",	no_argument,		0,	'h' },
	{ "verbose",	no_argument,		0,	'v' },
	{ 0,		0,			0,	0 },
};

static const char* usage_string =
	"Usage: %s [OPTION]... EXECUTABLE\n"
	"\n"
	"Monitor palmas_gpadc ADC channel and run EXECUTABLE when input changes.\n"
	"\n"
	"  -c, --channel     ADC channel to monitor. The Pyra volume wheel is\n"
	"                    connected to ADC channel 2.\n"
	"                    Defaults to " XSTR(DEFAULT_CHANNEL) ".\n"
	"  -l, --min         Lower limit to monitor. ADC values below this limit will\n"
	"                    only trigger executable once, until the ADC channel goes\n"
	"                    above the limit again. This level also takes into account\n"
	"                    the step value (see below), so the real lower threshold\n"
	"                    is increased by --step\n"
	"                    Defaults to " XSTR(DEFAULT_MIN) ".\n"
	"  -u, --max         Upper limit to monitor. ADC values above this limit will\n"
	"                    only trigger executable once, until the ADC cannel goes\n"
	"                    below the limit again.\n"
	"                    Defaults to " XSTR(DEFAULT_MAX) ".\n"
	"  -s, --step        How much the ADC input value is allowed to change before\n"
	"                    calling the executable again.\n"
	"                    Defaults to " XSTR(DEFAULT_STEP) ".\n"
	"  -t, --timeout     Check ADC value again after timeout ms. The timer is not\n"
	"                    re-started if the ADC value is unchanged.\n"
	"                    Setting the timeout to 0 disables this behavior.\n"
	"                    Defaults to " XSTR(DEFAULT_TIMEOUT) ".\n"
	"  -h, --help        Show this help text and exit\n"
	"  -v, --verbose     Be a bit more verbose.\n"
	"\n"
	"EXECUTABLE will be called like this:\n"
	"\n"
	"    EXECUTABLE <adc value> <min> <max>\n"
	"\n";

static int parse_number(const char* str)
{
	char *endptr;
	long val;
	int ret;
	
	errno = 0;
	val = strtol(str, &endptr, 0);
	
	if (errno != 0) {
		ret = errno;
		perror("strtol");
		return ret;
	}
	
	if (endptr == str) {
		fprintf(stderr, "'%s' is not a number\n", str);
		return -EINVAL;
	}
	
	if (val < 0 || val > INT_MAX) {
		fprintf(stderr, "%ld is not a valid (positive) number\n", val);
		return -EINVAL;
	}
	
	return val;
}

static void print_usage(const char* argv0)
{
	fprintf(stderr, usage_string, basename(argv0));
}

int pyra_get_config(struct pyra_volume_config *cfg, int argc, char *argv[])
{
	const char* argv0 = argv[0];

	if (argc <= 1) {
		goto err_no_executable;
	}

	cfg->channel = DEFAULT_CHANNEL;
	cfg->min = DEFAULT_MIN;
	cfg->max = DEFAULT_MAX;
	cfg->step = DEFAULT_STEP;
	cfg->timeout = DEFAULT_TIMEOUT;
	cfg->verbose = false;
	cfg->executable = NULL;

	while (1) {
		int c;
		int value;

		c = getopt_long(argc, argv, options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
			value = parse_number(optarg);
			if (value < 0)
				return value;
			cfg->channel = value;
			break;
		case 'l':
			value = parse_number(optarg);
			if (value < 0)
				return value;
			cfg->min = value;
			break;
		case 'u':
			value = parse_number(optarg);
			if (value < 0)
				return value;
			cfg->max = value;
			break;
		case 's':
			value = parse_number(optarg);
			if (value < 0)
				return value;
			cfg->step = value;
			break;
		case 't':
			value = parse_number(optarg);
			if (value < 0)
				return value;
			cfg->timeout = value;
			break;
		case 'h':
			print_usage(argv0);
			exit(0);
		case 'v':
			cfg->verbose = true;
			break;
		case '?':
		default:
			print_usage(argv0);
			return -EINVAL;
		}
	}

	if ((optind+1) != argc) {
		goto err_no_executable;
	}

	cfg->executable = argv[optind];

	if (cfg->verbose) {
		printf("Options:\n");
		printf("  channel:     %d\n", cfg->channel);
		printf("  lower limit: %d (real: %d)\n", cfg->min, cfg->min+cfg->step);
		printf("  upper limit: %d\n", cfg->max);
		printf("  step:        %d\n", cfg->step);
		printf("  timeout:     %d\n", cfg->timeout);
		puts  ("  verbose:     true (duh!)");
		printf("  executable:  \"%s\"\n", cfg->executable);
	}

	return 0;

err_no_executable:
	fprintf(stderr, "Missing executable\n");
	printf("Try '%s --help' for more information.\n", basename(argv0));
	return -EINVAL;
}
