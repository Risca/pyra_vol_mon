#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/iio/events.h>
#include <linux/iio/types.h>

#include "pyra_vol_mon.h"

static bool event_is_ours(struct iio_event_data *event, int channel)
{
	int chan = IIO_EVENT_CODE_EXTRACT_CHAN(event->id);
	enum iio_chan_type type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event->id);
	enum iio_event_type ev_type = IIO_EVENT_CODE_EXTRACT_TYPE(event->id);
	enum iio_event_direction dir = IIO_EVENT_CODE_EXTRACT_DIR(event->id);

	if (chan != channel)
		return false;
 	if (type != IIO_VOLTAGE)
		return false;
	if (ev_type != IIO_EV_TYPE_THRESH)
		return false;
	if (dir != IIO_EV_DIR_FALLING && dir != IIO_EV_DIR_RISING)
		return false;

	return true;
}

static int execute_callback(const struct pyra_volume_config *config, int value)
{
	pid_t child;
	char value_string[32];
	char min_string[32];
	char max_string[32];
	char *const argv[] = {
		(char*)config->executable,
		value_string,
		min_string,
		max_string,
		NULL,
	};

	snprintf(value_string, sizeof(value_string), "%d", value);
	snprintf(min_string, sizeof(min_string), "%d", config->min);
	snprintf(max_string, sizeof(max_string), "%d", config->max);

	child = fork();
	if (child > 0) {
		waitpid(child, NULL, 0);
		return 0;
	}
	if (child < 0) {
		perror("fork failed");
		return -1;
	}
	execvp(config->executable, argv);
	perror("child process execve failed");
	exit(1);
}

static int read_event(int event_fd, int channel)
{
	struct iio_event_data event;
	int ret;

	do {
		ret = read(event_fd, &event, sizeof(event));
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			else if (errno == EAGAIN) {
				ret = 0;
				fprintf(stderr, "nothing available\n");
			} else {
				ret = -errno;
				perror("Failed to read event from device");
			}
		}
		else if (ret != sizeof(event)) {
			fprintf(stderr, "Reading event failed!\n");
			ret = -EIO;
		}
		else {
			ret = event_is_ours(&event, channel);
		}
	} while (false);

	return ret;
}

static int read_timer(int fd)
{
	ssize_t s;
	uint64_t exp;

	do {
		s = read(fd, &exp, sizeof(uint64_t));
		if (s == sizeof(uint64_t))
			return 0; // all is as expected
		else if (s < 0) {
			if (errno == EINTR)
				continue;
			return errno;
		}
		else { // s is [0, sizeof(uint64_t)[
			fprintf(stderr, "Short read of timer fd\n");
			return -1;
		}
	} while (false);

	return 0;
}

static int reset_timer(int fd, long ms)
{
	const struct itimerspec ts = {
		.it_interval = { 0, 0 },
		.it_value = {
			.tv_sec = ms / 1000,
			.tv_nsec = (ms % 1000) * 1000 * 1000,
		},
	};

	return timerfd_settime(fd, 0, &ts, NULL);
}

static int stop_timer(int fd)
{
	static const struct itimerspec ts = { 0 };
	return timerfd_settime(fd, 0, &ts, NULL);
}

int main(int argc, char **argv)
{
	int ret;
	int event_fd;
	int timer_fd = -1;
	int num_fds = 1;
	int last_value;
	struct pyra_volume_config config;
	struct pyra_iio_event_handle *iio_event_handle;
	struct pollfd pfds[2];

	ret = pyra_get_config(&config, argc, argv);
	if (ret < 0)
		return ret;

	event_fd = pyra_iio_event_open(&iio_event_handle, config.channel);
	if (event_fd < 0)
		return event_fd;
	pfds[0].fd = event_fd;
	pfds[0].events = POLLIN;

	if (config.timeout) {
		timer_fd = timerfd_create(CLOCK_BOOTTIME, TFD_CLOEXEC);
		if (timer_fd < 0) {
			perror("timerfd_create");
			close(event_fd);
			return timer_fd;
		}
		pfds[1].fd = timer_fd;
		pfds[1].events = POLLIN;
		num_fds++;
	}

	ret = read_value_and_update_thresholds(&config, iio_event_handle);
	if (ret >= 0)
		execute_callback(&config, ret);

	last_value = ret;
	while (true) {
		bool read_and_update = false;
		int ready;

		ready = poll(pfds, num_fds, -1);
		if (ready == -1) {
			perror("poll");
			continue;
		}

		for (int i = 0; i < num_fds; ++i) {
			if (!pfds[i].revents)
				continue;

			if (!(pfds[i].revents & POLLIN)) {
				fprintf(stderr, "unexpected revent for fd %d: %d\n",
						pfds[i].fd, pfds[i].revents);
				continue;
			}

			if (pfds[i].fd == event_fd) {
				ret = read_event(event_fd, config.channel);
				if (ret < 0) // abort on errors
					goto out;
				read_and_update = ret;
			}
			else if (pfds[i].fd == timer_fd) {
				ret = read_timer(timer_fd);
				if (ret < 0) // abort on errors
					goto out;
				read_and_update = true;
			}
			else {
				fprintf(stderr, "POLLIN on unknown fd: %d\n", pfds[i].fd);
				continue;
			}
		}

		if (read_and_update) {
			int value = read_value_and_update_thresholds(&config, iio_event_handle);
			if (value >= 0 && value != last_value) {
				last_value = value;
				execute_callback(&config, value);
				if (config.timeout) {
					if (value > config.min && value < config.max)
						ret = reset_timer(timer_fd, config.timeout);
					else
						ret = stop_timer(timer_fd);
					if (ret < 0) // abort on errors
						goto out;
				}
			}
		}
	}

out:
	if (close(timer_fd) == -1)
		perror("Failed to close timer fd");
	if (close(event_fd) == -1)
		perror("Failed to close event file");

	pyra_iio_event_free(iio_event_handle);

	return ret;
}
