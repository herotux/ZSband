/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware diagnostic for the Xiaomi Mi Band 4 RM69310 display path.
 * CI compiles this sample only; it does not flash or run it on hardware.
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
#define BAND_HEIGHT 40U

/*
 * Keep the complete frame in RAM and submit it in one display_write().
 * The SmartBond MIPI-DBI host may still be transferring a previous write
 * when the next one arrives; issuing 240 one-row transfers back-to-back can
 * therefore fail with -EBUSY and leave only part of the test pattern drawn.
 */
static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

static uint16_t color_for_row(uint16_t y)
{
	if (y < BAND_HEIGHT) {
		return 0xF800U; /* red */
	}
	if (y < 2U * BAND_HEIGHT) {
		return 0x07E0U; /* green */
	}
	if (y < 3U * BAND_HEIGHT) {
		return 0x001FU; /* blue */
	}
	if (y < 4U * BAND_HEIGHT) {
		return 0xFFE0U; /* yellow */
	}
	if (y < 5U * BAND_HEIGHT) {
		return 0xFFFFU; /* white */
	}

	return 0x0000U; /* black */
}

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
	struct display_capabilities caps = {0};
	struct display_buffer_descriptor desc = {
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.pitch = DISPLAY_WIDTH,
		.buf_size = sizeof(framebuffer),
	};
	int ret;

	LOG_INF("Mi Band 4 display diagnostic starting");

	if (!device_is_ready(display)) {
		LOG_ERR("Chosen display device is not ready");
		return 1;
	}

	display_get_capabilities(display, &caps);
	LOG_INF("Display capabilities: %u x %u, formats=0x%x, current=%d",
		caps.x_resolution, caps.y_resolution,
		caps.supported_pixel_formats, caps.current_pixel_format);

	if (caps.x_resolution != DISPLAY_WIDTH || caps.y_resolution != DISPLAY_HEIGHT) {
		LOG_ERR("Unexpected resolution; expected %u x %u",
			DISPLAY_WIDTH, DISPLAY_HEIGHT);
		return 1;
	}

	if ((caps.supported_pixel_formats & PIXEL_FORMAT_RGB_565) == 0U) {
		LOG_ERR("RGB565 is not reported as supported");
		return 1;
	}

	ret = display_set_pixel_format(display, PIXEL_FORMAT_RGB_565);
	if (ret < 0) {
		LOG_ERR("Could not select RGB565: %d", ret);
		return 1;
	}

	/* Construct six horizontal RGB565 bands in the full-frame buffer. */
	for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
		const uint16_t color = color_for_row(y);

		for (size_t x = 0; x < DISPLAY_WIDTH; x++) {
			framebuffer[(size_t)y * DISPLAY_WIDTH + x] = color;
		}
	}

	ret = display_blanking_off(display);
	if (ret < 0) {
		LOG_ERR("Could not unblank display: %d", ret);
		return 1;
	}

	/* Submit one full frame to avoid overlapping asynchronous DBI transfers. */
	ret = display_write(display, 0, 0, &desc, framebuffer);
	if (ret < 0) {
		LOG_ERR("Full-frame RGB565 write failed: %d", ret);
		return 1;
	}

	LOG_INF("Full-frame RGB565 write submitted: %u x %u, %u bytes",
		DISPLAY_WIDTH, DISPLAY_HEIGHT, (unsigned int)sizeof(framebuffer));
	LOG_INF("Pattern order from top: red, green, blue, yellow, white, black");
	LOG_INF("Visual correctness still requires observation on the physical band");
	return 0;
}
