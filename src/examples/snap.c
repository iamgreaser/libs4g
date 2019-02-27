#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>

#include "s4g.h"

int main(int argc, char *argv[])
{
	s4g_display_t *S = NULL;
	s4g_window_t *W = NULL;
	S = s4g_open(NULL);
	if(S == NULL) {
		goto fail;
	}

	//W = s4g_open_window_at_cursor(S);
	//W = s4g_open_window_with_title(S, "Untitled - Notepad");
	W = s4g_open_root_window(S);
	if(W == NULL) {
		goto fail;
	}

	for(;;) {
		uint8_t *data;
		int width, height, bytes_per_line;
		bool did_take_snapshot = s4g_snap_from_window(W, (void **)&data, &width, &height, &bytes_per_line);
		if(!did_take_snapshot) {
			goto fail;
		}
		//printf("snapped %p %d %d %d\n", data, width, height, bytes_per_line);

		struct timeval tv;
		gettimeofday(&tv, NULL);

		int sum_r = 0;
		int sum_g = 0;
		int sum_b = 0;
		for(int y = 0; y < height; y++) {
			uint32_t *row = (uint32_t *)&data[y*bytes_per_line];
			for(int x = 0; x < width; x++) {
				sum_r += (row[x]>>16)&0xFF;
				sum_g += (row[x]>>8)&0xFF;
				sum_b += (row[x])&0xFF;
			}
		}
		printf("%d.%06d Snap %p %d %d %d\n", tv.tv_sec, tv.tv_usec, data, sum_r, sum_g, sum_b);
		usleep(1000);
	}

	success:
	s4g_close_window(W);
	s4g_close(S);
	return 0;

	fail:
	s4g_close_window(W);
	s4g_close(S);
	return 1;
}

