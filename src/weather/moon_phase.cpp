// Moon phase calculation.
// From the CYD weather station / Bodmer's OpenWeather library, which adapted it from
// http://www.voidware.com/moon_phase.htm
#include <math.h>
#include <time.h>

#include "weather.h"
#include "weather_icons.h"

namespace weather {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kRad = kPi / 180.0;
constexpr double kSmall = 1e-12;

double julian(int year, int month, double day) {
    int b = 0;
    if (month < 3) {
        year--;
        month += 12;
    }
    if (year > 1582 || (year == 1582 && month > 10) || (year == 1582 && month == 10 && day > 15)) {
        const int a = year / 100;
        b = 2 - a + a / 4;
    }
    const int c = 365.25 * year;
    const int e = 30.6001 * (month + 1);
    return b + c + e + day + 1720994.5;
}

double sunPosition(double j) {
    double n = 360 / 365.2422 * j;
    int i = n / 360;
    n = n - i * 360.0;
    double x = n - 3.762863;
    if (x < 0) x += 360;
    x *= kRad;
    double e = x, dl;
    do {
        dl = e - .016718 * sin(e) - x;
        e = e - dl / (1 - .016718 * cos(e));
    } while (fabs(dl) >= kSmall);
    const double v = 360 / kPi * atan(1.01686011182 * tan(e / 2));
    double l = v + 282.596403;
    i = l / 360;
    l = l - i * 360.0;
    return l;
}

double moonPosition(double j, double ls) {
    double ms = 0.985647332099 * j - 3.762863;
    if (ms < 0) ms += 360.0;
    double l = 13.176396 * j + 64.975464;
    int i = l / 360;
    l = l - i * 360.0;
    if (l < 0) l += 360.0;
    double mm = l - 0.1114041 * j - 349.383063;
    i = mm / 360;
    mm -= i * 360.0;
    const double ev = 1.2739 * sin((2 * (l - ls) - mm) * kRad);
    const double sms = sin(ms * kRad);
    const double ae = 0.1858 * sms;
    mm += ev - ae - 0.37 * sms;
    const double ec = 6.2886 * sin(mm * kRad);
    l += ev + ec - ae + 0.214 * sin(2 * mm * kRad);
    l = 0.6583 * sin(2 * (l - ls) * kRad) + l;
    return l;
}

// Sun-moon elongation in degrees (0 = new, 180 = full).
double elongation(time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    const double hour = tm.tm_hour + tm.tm_min / 60.0;
    const double j = julian(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday + hour / 24.0) - 2444238.5;
    const double ls = sunPosition(j);
    const double lm = moonPosition(j, ls);
    double e = fmod(lm - ls, 360.0);
    if (e < 0) e += 360;
    return e;
}

const lv_image_dsc_t *const kMoon[24] = {
    &wi_moon_moonphase_l0,  &wi_moon_moonphase_l1,  &wi_moon_moonphase_l2,  &wi_moon_moonphase_l3,
    &wi_moon_moonphase_l4,  &wi_moon_moonphase_l5,  &wi_moon_moonphase_l6,  &wi_moon_moonphase_l7,
    &wi_moon_moonphase_l8,  &wi_moon_moonphase_l9,  &wi_moon_moonphase_l10, &wi_moon_moonphase_l11,
    &wi_moon_moonphase_l12, &wi_moon_moonphase_l13, &wi_moon_moonphase_l14, &wi_moon_moonphase_l15,
    &wi_moon_moonphase_l16, &wi_moon_moonphase_l17, &wi_moon_moonphase_l18, &wi_moon_moonphase_l19,
    &wi_moon_moonphase_l20, &wi_moon_moonphase_l21, &wi_moon_moonphase_l22, &wi_moon_moonphase_l23,
};

}  // namespace

const lv_image_dsc_t *moonIcon(time_t t) {
    // Same mapping as the original sketch: image L23 is the new moon.
    return kMoon[((int)((elongation(t) + 7.5) / 15) + 23) % 24];
}

const char *moonPhaseName(time_t t) {
    static const char *const kNames[8] = {"New moon",  "Waxing crescent", "First quarter", "Waxing gibbous",
                                          "Full moon", "Waning gibbous",  "Last quarter",  "Waning crescent"};
    return kNames[(int)((elongation(t) + 22.5) / 45) & 7];
}

int moonIllumination(time_t t) {
    return (int)(100.0 * (1.0 - cos(elongation(t) * kRad)) / 2 + 0.5);
}

}  // namespace weather
