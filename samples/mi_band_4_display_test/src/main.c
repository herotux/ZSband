/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mi Band 4 Jalali clock prototype.
 *
 * IMPORTANT: the DA14697 board currently has no validated RTC or phone-time
 * synchronization. The clock therefore starts from this firmware's build
 * timestamp (treated as UTC) plus Iran Standard Time (+03:30), then advances
 * using uptime. It is useful for validating the UI/display, not as an accurate
 * time source after flashing. Do not represent it as a production clock yet.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(mi_band_4_jalali_clock, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH 120U
#define DISPLAY_HEIGHT 240U
#define TIMEZONE_OFFSET_SECONDS (3 * 60 * 60 + 30 * 60)
#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3))

#define COLOR_BG       RGB565(8, 13, 25)
#define COLOR_PANEL    RGB565(18, 28, 47)
#define COLOR_TEXT     RGB565(246, 249, 255)
#define COLOR_MUTED    RGB565(143, 161, 184)
#define COLOR_ACCENT   RGB565(56, 214, 190)
#define COLOR_RULE     RGB565(38, 57, 80)

static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

/* Compact 5x7 bitmap font for digits and punctuation. */
static const uint8_t glyphs[12][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
    {0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00}, /* : */
    {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00}, /* / */
};

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int x0 = MAX(x, 0);
    int y0 = MAX(y, 0);
    int x1 = MIN(x + w, (int)DISPLAY_WIDTH);
    int y1 = MIN(y + h, (int)DISPLAY_HEIGHT);

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            framebuffer[(size_t)py * DISPLAY_WIDTH + (size_t)px] = color;
        }
    }
}

static void draw_char(char c, int x, int y, int scale, uint16_t color)
{
    int index;

    if (c >= '0' && c <= '9') {
        index = c - '0';
    } else if (c == ':') {
        index = 10;
    } else if (c == '/') {
        index = 11;
    } else {
        return;
    }

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if ((glyphs[index][row] & (1U << (4 - col))) != 0U) {
                fill_rect(x + col * scale, y + row * scale,
                          scale, scale, color);
            }
        }
    }
}

static int text_width(const char *s, int scale)
{
    int len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len == 0 ? 0 : len * 5 * scale + (len - 1) * scale;
}

static void draw_text(const char *s, int y, int scale, uint16_t color)
{
    int x = ((int)DISPLAY_WIDTH - text_width(s, scale)) / 2;

    while (*s != '\0') {
        draw_char(*s, x, y, scale, color);
        x += 6 * scale;
        s++;
    }
}

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int month_number(const char *mon)
{
    static const char *const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    for (int i = 0; i < 12; i++) {
        const char *a = mon;
        const char *b = months[i];
        if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
            return i + 1;
        }
    }
    return 1;
}

static int64_t build_epoch_seconds(void)
{
    /* __DATE__ is "Mmm dd yyyy"; __TIME__ is "hh:mm:ss". */
    const char date[] = __DATE__;
    const char time[] = __TIME__;
    int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
               (date[9] - '0') * 10 + (date[10] - '0');
    int month = month_number(date);
    int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
    int hour = (time[0] - '0') * 10 + (time[1] - '0');
    int minute = (time[3] - '0') * 10 + (time[4] - '0');
    int second = (time[6] - '0') * 10 + (time[7] - '0');
    int64_t days = 0;

    for (int y = 1970; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    for (int m = 1; m < month; m++) {
        static const uint8_t month_days[] =
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        days += month_days[m - 1];
        if (m == 2 && is_leap_year(year)) {
            days++;
        }
    }

    days += day - 1;
    return days * 86400 + hour * 3600 + minute * 60 + second;
}

static void gregorian_from_epoch(int64_t epoch, int *year, int *month,
                                 int *day, int *hour, int *minute, int *second)
{
    int64_t days = epoch / 86400;
    int64_t seconds = epoch % 86400;

    if (seconds < 0) {
        seconds += 86400;
        days--;
    }

    *hour = (int)(seconds / 3600);
    *minute = (int)((seconds % 3600) / 60);
    *second = (int)(seconds % 60);

    *year = 1970;
    while (days >= (is_leap_year(*year) ? 366 : 365)) {
        days -= is_leap_year(*year) ? 366 : 365;
        (*year)++;
    }

    *month = 1;
    while (true) {
        int mdays;
        switch (*month) {
        case 2: mdays = is_leap_year(*year) ? 29 : 28; break;
        case 4: case 6: case 9: case 11: mdays = 30; break;
        default: mdays = 31; break;
        }
        if (days < mdays) {
            break;
        }
        days -= mdays;
        (*month)++;
    }
    *day = (int)days + 1;
}

/* Gregorian to Jalali conversion using the 12053-day / 1461-day cycles. */
static void gregorian_to_jalali(int gy, int gm, int gd,
                                int *jy, int *jm, int *jd)
{
    static const int cumulative_days[] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int gy2 = gm > 2 ? gy + 1 : gy;
    int64_t days = 355666LL + 365LL * gy +
        (gy2 + 3) / 4 - (gy2 + 99) / 100 + (gy2 + 399) / 400 +
        gd + cumulative_days[gm - 1];

    *jy = -1595 + 33 * (int)(days / 12053);
    days %= 12053;
    *jy += 4 * (int)(days / 1461);
    days %= 1461;

    if (days > 365) {
        *jy += (int)((days - 1) / 365);
        days = (days - 1) % 365;
    }

    if (days < 186) {
        *jm = 1 + (int)(days / 31);
        *jd = 1 + (int)(days % 31);
    } else {
        *jm = 7 + (int)((days - 186) / 30);
        *jd = 1 + (int)((days - 186) % 30);
    }
}

static void format_number(char *out, int value, int digits)
{
    for (int i = digits - 1; i >= 0; i--) {
        out[i] = (char)('0' + value % 10);
        value /= 10;
    }
    out[digits] = '\0';
}

static int render_clock(const struct device *display, int64_t epoch)
{
    int gy, gm, gd, hour, minute, second;
    int jy, jm, jd;
    char time_text[9];
    char date_text[11];
    struct display_buffer_descriptor desc = {
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .pitch = DISPLAY_WIDTH,
        .buf_size = sizeof(framebuffer),
    };

    gregorian_from_epoch(epoch, &gy, &gm, &gd, &hour, &minute, &second);
    gregorian_to_jalali(gy, gm, gd, &jy, &jm, &jd);

    format_number(&time_text[0], hour, 2);
    time_text[2] = ':';
    format_number(&time_text[3], minute, 2);
    time_text[5] = ':';
    format_number(&time_text[6], second, 2);
    time_text[8] = '\0';

    format_number(&date_text[0], jy, 4);
    date_text[4] = '/';
    format_number(&date_text[5], jm, 2);
    date_text[7] = '/';
    format_number(&date_text[8], jd, 2);
    date_text[10] = '\0';

    for (size_t i = 0; i < ARRAY_SIZE(framebuffer); i++) {
        framebuffer[i] = COLOR_BG;
    }

    fill_rect(8, 16, 104, 2, COLOR_RULE);
    fill_rect(8, 17, 4, 20, COLOR_ACCENT);
    draw_text("1405", 28, 1, COLOR_MUTED);
    draw_text(time_text, 78, 2, COLOR_TEXT);
    fill_rect(24, 111, 72, 1, COLOR_RULE);
    draw_text(date_text, 132, 1, COLOR_ACCENT);

    /* Month/day-independent progress accent; date is explicitly Jalali. */
    fill_rect(18, 171, 84, 8, COLOR_PANEL);
    fill_rect(18, 171, (jm * 84) / 12, 8, COLOR_ACCENT);
    draw_text("07", 196, 1, COLOR_MUTED);
    draw_text("18", 196, 1, COLOR_MUTED);
    fill_rect(8, 222, 104, 2, COLOR_RULE);

    int ret = display_write(display, 0, 0, &desc, framebuffer);
    if (ret < 0) {
        LOG_ERR("Display write failed: %d", ret);
    }

    return ret;
}

int main(void)
{
    const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
    int ret;

    LOG_INF("Mi Band 4 Jalali clock prototype starting");
    LOG_WRN("Clock starts at build timestamp; no RTC/phone time sync is implemented");

    if (!device_is_ready(display)) {
        LOG_ERR("Chosen display device is not ready");
        return 1;
    }

    ret = display_set_pixel_format(display, PIXEL_FORMAT_RGB_565);
    if (ret < 0) {
        LOG_ERR("Could not select RGB565: %d", ret);
        return 1;
    }

    ret = display_blanking_off(display);
    if (ret < 0) {
        LOG_ERR("Could not unblank display: %d", ret);
        return 1;
    }

    const int64_t base_epoch = build_epoch_seconds() + TIMEZONE_OFFSET_SECONDS;
    const int64_t start_uptime = k_uptime_get() / 1000;

    while (true) {
        int64_t elapsed = k_uptime_get() / 1000 - start_uptime;
        ret = render_clock(display, base_epoch + elapsed);
        if (ret < 0) {
            return 1;
        }
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
