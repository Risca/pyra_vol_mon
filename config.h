#pragma once

#include <stdbool.h>

struct pyra_volume_config {
	unsigned int	channel;
	unsigned int	min;
	unsigned int	max;
	unsigned int	step;
	unsigned int    timeout;
	bool		verbose;
	const char*	executable;
};

int pyra_get_config(struct pyra_volume_config *cfg, int argc, char *argv[]);
