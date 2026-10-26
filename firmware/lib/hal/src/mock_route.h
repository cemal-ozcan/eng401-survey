#pragma once

// Synthetic survey route for the mock GNSS: a closed polyline in local
// East/North metres around an origin, walked at constant speed. The same
// object gives the ground truth, so analysis scripts can be tested against
// a known reference trajectory.

#include <cstddef>

namespace hal {

struct LocalPoint {
  double e;  // m east of origin
  double n;  // m north of origin
};

struct RoutePose {
  double lat, lon;
  double headingDeg;  // 0 = north, clockwise
};

class MockRoute {
 public:
  // Default: ~720 m loop near (39.8900 N, 32.7800 E). Synthetic, not a real campus path.
  MockRoute();
  MockRoute(double originLat, double originLon, const LocalPoint* points, size_t count);

  double lengthM() const { return length_; }
  // Pose after walking `distanceM` along the loop (wraps around).
  RoutePose poseAt(double distanceM) const;
  // Local metres -> WGS84 (equirectangular approximation, fine for < few km).
  void toLatLon(double e, double n, double& lat, double& lon) const;

  double originLat() const { return lat0_; }
  double originLon() const { return lon0_; }

  static constexpr size_t kMaxPoints = 16;

 private:
  double lat0_, lon0_;
  LocalPoint pts_[kMaxPoints];
  size_t count_ = 0;
  double length_ = 0.0;
};

}  // namespace hal
