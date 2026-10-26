#include "utm.h"

#include <cmath>

namespace geo {

namespace {

// WGS84 ellipsoid and UTM projection constants
constexpr double kA = 6378137.0;
constexpr double kF = 1.0 / 298.257223563;
constexpr double kK0 = 0.9996;
constexpr double kFalseEasting = 500000.0;
constexpr double kFalseNorthingSouth = 10000000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

// Series coefficients depend only on the ellipsoid: compute once.
struct Coefficients {
  double e;          // first eccentricity
  double rectA;      // rectifying radius A
  double alpha[7];   // alpha[1..6]
};

Coefficients makeCoefficients() {
  Coefficients c{};
  const double n = kF / (2.0 - kF);
  const double n2 = n * n, n3 = n2 * n, n4 = n3 * n, n5 = n4 * n, n6 = n5 * n;
  c.e = std::sqrt(kF * (2.0 - kF));
  c.rectA = kA / (1.0 + n) * (1.0 + n2 / 4.0 + n4 / 64.0 + n6 / 256.0);
  c.alpha[1] = n / 2.0 - 2.0 * n2 / 3.0 + 5.0 * n3 / 16.0 + 41.0 * n4 / 180.0 - 127.0 * n5 / 288.0 +
               7891.0 * n6 / 37800.0;
  c.alpha[2] = 13.0 * n2 / 48.0 - 3.0 * n3 / 5.0 + 557.0 * n4 / 1440.0 + 281.0 * n5 / 630.0 -
               1983433.0 * n6 / 1935360.0;
  c.alpha[3] = 61.0 * n3 / 240.0 - 103.0 * n4 / 140.0 + 15061.0 * n5 / 26880.0 + 167603.0 * n6 / 181440.0;
  c.alpha[4] = 49561.0 * n4 / 161280.0 - 179.0 * n5 / 168.0 + 6601661.0 * n6 / 7257600.0;
  c.alpha[5] = 34729.0 * n5 / 80640.0 - 3418889.0 * n6 / 1995840.0;
  c.alpha[6] = 212378941.0 * n6 / 319334400.0;
  return c;
}

const Coefficients& coeffs() {
  static const Coefficients c = makeCoefficients();
  return c;
}

}  // namespace

uint8_t utmZone(double lat, double lon) {
  if (!(lat >= -80.0 && lat <= 84.0) || !(lon >= -180.0 && lon <= 180.0)) return 0;
  int zone = static_cast<int>(std::floor((lon + 180.0) / 6.0)) + 1;
  if (zone > 60) zone = 60;  // lon == 180

  if (lat >= 56.0 && lat < 64.0 && lon >= 3.0 && lon < 12.0) zone = 32;  // Norway
  if (lat >= 72.0) {                                                      // Svalbard
    if (lon >= 0.0 && lon < 9.0) zone = 31;
    else if (lon >= 9.0 && lon < 21.0) zone = 33;
    else if (lon >= 21.0 && lon < 33.0) zone = 35;
    else if (lon >= 33.0 && lon < 42.0) zone = 37;
  }
  return static_cast<uint8_t>(zone);
}

bool latLonToUtmInZone(double lat, double lon, uint8_t zone, Utm& out) {
  if (zone < 1 || zone > 60 || !(lat >= -80.0 && lat <= 84.0) || !(lon >= -180.0 && lon <= 180.0))
    return false;
  const Coefficients& c = coeffs();

  const double lon0 = (zone - 1) * 6.0 - 180.0 + 3.0;  // central meridian
  double dLon = lon - lon0;
  if (dLon > 180.0) dLon -= 360.0;  // forced zone across the antimeridian
  if (dLon < -180.0) dLon += 360.0;

  const double phi = lat * kDegToRad;
  const double lam = dLon * kDegToRad;

  // Conformal latitude via tau' (Karney eq. 7-9)
  const double sinPhi = std::sin(phi);
  const double t = std::sinh(std::atanh(sinPhi) - c.e * std::atanh(c.e * sinPhi));
  const double xiP = std::atan2(t, std::cos(lam));
  const double etaP = std::atanh(std::sin(lam) / std::sqrt(1.0 + t * t));

  double xi = xiP, eta = etaP;
  for (int j = 1; j <= 6; ++j) {
    xi += c.alpha[j] * std::sin(2.0 * j * xiP) * std::cosh(2.0 * j * etaP);
    eta += c.alpha[j] * std::cos(2.0 * j * xiP) * std::sinh(2.0 * j * etaP);
  }

  out.zone = zone;
  out.hemisphere = lat >= 0.0 ? 'N' : 'S';
  out.easting = kFalseEasting + kK0 * c.rectA * eta;
  out.northing = kK0 * c.rectA * xi + (lat < 0.0 ? kFalseNorthingSouth : 0.0);
  return true;
}

bool latLonToUtm(double lat, double lon, Utm& out) {
  const uint8_t zone = utmZone(lat, lon);
  return zone != 0 && latLonToUtmInZone(lat, lon, zone, out);
}

}  // namespace geo
