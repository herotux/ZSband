/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compile-time smoke test for the Xiaomi Mi Band 4 RM69310 display path.
 * This sample is intentionally not flashed by CI.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(mi_band_4_display_test, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH 120U
#define DISPLAY_HEIGHT 240U

static uint16_t line[DISPLAY_WIDTH];

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
	struct display_capabilities caps;
	struct display_buffer_descriptor desc = {
		.width = DISPLAY_WIDTH,
		.height = 1,
		.pitch = DISPLAY_WIDTH,
		.buf_size = sizeof(line),
	};
	int ret;

	if (!device_is_ready(display)) {
		LOG_ERR("Chosen display device is not ready");
		return 0;
	}

	display_get_capabilities(display, &caps);
	LOG_INF("Display ready: %u x %u, formats=0x%x",
		caps.x_resolution, caps.y_resolution,
		caps.supported_pixel_formats);

	ret = display_blanking_off(display);
	if (ret < 0) {
		LOG_ERR("Could not unblank display: %d", ret);
		return 0;
	}

	/*
	 * Generate a simple RGB565 color-bar pattern one scanline at a time.
	 * This checks that the display API and RM69310 driver compile together.
	 */
	for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
		uint16_t color;

		if (y < 40U) {
			color = 0xF800U; /* red */
		} else if (y < 80U) {
			color = 0x07E0U; /* green */
		} else if (y < 120U) {
			color = 0x001FU; /* blue */
		} else if (y < 160U) {
			color = 0xFFE0U; /* yellow */
		} else if (y < 200U) {
			color = 0xFFFFU; /* white */
		} else {
			color = 0x0000U; /* black */
		}

		for (size_t x = 0; x < ARRAY_SIZE(line); x++) {
			line[x] = color;
		}

		ret = display_write(display, 0, y, &desc, line);
		if (ret < 0) {
			LOG_ERR("Display write failed at row %u: %d", y, ret);
			return 0;
		}
	}

	LOG_INF("Color-bar pattern submitted to display");
	return 0;
}
