#include <stdio.h>
#include <errno.h>

#include "pyra_vol_mon.h"

int read_value_and_update_thresholds(
		const struct pyra_volume_config *config,
		const struct pyra_iio_event_handle *iio)
{
	int ret;
	int value;
	int low = -1;
	int high = -1;

	ret = pyra_iio_get_value(iio);
	if (ret < 0) {
		fprintf(stderr, "Error reading current value: %d\n", ret);
		return -EAGAIN;
	}

	value = ret;

	/* clamp value to [min, max] */
	if (value > config->max)
		value = config->max;
	else if (value < config->min)
		value = config->min;

	/* update upper threshold */
	if (value == config->max) {
		ret = pyra_iio_disable_upper_threshold(iio);
		if (ret < 0)
			fprintf(stderr, "Failed to disable upper threshold %d: %d\n", value, ret);
	} else {
		high = value + config->step;
		if (high > (int)config->max)
			high = config->max - 1;	// set to last step

		ret = pyra_iio_enable_upper_threshold(iio, high);
		if (ret < 0)
			fprintf(stderr, "Failed to enable upper threshold %d: %d\n", high, ret);
	}

	/* update lower threshold */
	if (value == config->min) {
		ret = pyra_iio_disable_lower_threshold(iio);
		if (ret < 0)
			fprintf(stderr, "Failed to disable lower threshold %d: %d\n", value, ret);
	} else {
		low = value - config->step;
		if (low < (int)config->min)
			low = config->min + 1;	// set to last step

		ret = pyra_iio_enable_lower_threshold(iio, low);
		if (ret < 0)
			fprintf(stderr, "Failed to enable lower threshold %d: %d\n", low, ret);
	}

	fprintf(stderr, "value %d low threshold %d high threshold %d min %d max %d\n", value, low, high, config->min, config->max);

	return value;
}
