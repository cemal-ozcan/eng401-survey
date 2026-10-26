#include "mock_route.h"

#include <cmath>

namespace hal {

namespace {
constexpr double kMetersPerDegLat = 111320.0;
constexpr double kPi = 3.14159265358979323846;
const LocalPoint kDefaultLoop[] = {{0, 0}, {200, 0}, {200, 120}, {80, 160}, {0, 120}, {0, 0}};
}  // namespace

MockRoute::MockRoute()
    : MockRoute(39.8900, 32.7800, kDefaultLoop, sizeof(kDefaultLoop) / sizeof(kDefaultLoop[0])) {}

MockRoute::MockRoute(double originLat, double originLon, const LocalPoint* points, size_t count)
    : lat0_(originLat), lon0_(originLon) {
  count_ = count > kMaxPoints ? kMaxPoints : count;
  for (size_t i = 0; i < count_; ++i) pts_[i] = points[i];
  for (size_t i = 1; i < count_; ++i)
    length_ += std::hypot(pts_[i].e - pts_[i - 1].e, pts_[i].n - pts_[i - 1].n);
}

void MockRoute::toLatLon(double e, double n, double& lat, double& lon) const {
  lat = lat0_ + n / kMetersPerDegLat;
  lon = lon0_ + e / (kMetersPerDegLat * std::cos(lat0_ * kPi / 180.0));
}

RoutePose MockRoute::poseAt(double distanceM) const {
  RoutePose pose{lat0_, lon0_, 0.0};
  if (count_ < 2 || length_ <= 0.0) return pose;
  double d = std::fmod(distanceM, length_);
  if (d < 0) d += length_;
  for (size_t i = 1; i < count_; ++i) {
    double de = pts_[i].e - pts_[i - 1].e, dn = pts_[i].n - pts_[i - 1].n;
    double seg = std::hypot(de, dn);
    if (d <= seg || i == count_ - 1) {
      double f = seg > 0 ? d / seg : 0.0;
      toLatLon(pts_[i - 1].e + f * de, pts_[i - 1].n + f * dn, pose.lat, pose.lon);
      double h = std::atan2(de, dn) * 180.0 / kPi;
      pose.headingDeg = h < 0 ? h + 360.0 : h;
      return pose;
    }
    d -= seg;
  }
  return pose;
}

}  // namespace hal
