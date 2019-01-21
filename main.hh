#ifndef __SATNOW_MAIN_HH
#define __SATNOW_MAIN_HH

#include <CoordTopocentric.h>
#include <DateTime.h>
#include <Observer.h>
#include <SGP4.h>
#include <algorithm>
#include "db.hh"

// Version info. Excuse the ugly trick to get strigification for a macro value.
#define MAJOR 0
#define MINOR 1
#define PATCH 0
#define _VER2(_x, _y, _z) #_x "." #_y "." #_z
#define _VER(_x, _y, _z) _VER2(_x, _y, _z)
#define VER _VER(MAJOR, MINOR, PATCH)

// TLE and look angle pair.
using SatLookAngle = std::pair<Tle, CoordTopocentric>;

// Container class for holding Tle and look angles.
class SatLookAngles {
 private:
  double _lat, _lon, _alt;
  std::vector<SatLookAngle> _sats;
  Observer _me;
  DateTime _time;

 public:
  SatLookAngles(double lat, double lon, double alt)
      : _me(lat, lon, alt), _time(DateTime::Now(true)) {}

  // Add the tle to the _sats container, and also
  // generate the look angle at _time.
  void add(Tle tle) {
    const auto model = SGP4(tle);
    const auto pos = model.FindPosition(_time);
    const auto la = _me.GetLookAngle(pos);
    _sats.emplace_back(tle, la);
  }

  // Regenerate new look angles with the current time.
  void updateTimeAndPositions() {
    _time = DateTime::Now(true);
    for (auto &sat : _sats) {
      const auto model = SGP4(sat.first);
      const auto pos = model.FindPosition(_time);
      sat.second = _me.GetLookAngle(pos);
    }
  }

  // Sort the satellites based on range (closest to furthest).
  void sort() {
    std::sort(_sats.begin(), _sats.end(),
              [](const SatLookAngle &a, const SatLookAngle &b) {
                return a.second.range < b.second.range;
              });
  }

  std::vector<SatLookAngle>::iterator begin() { return _sats.begin(); }
  std::vector<SatLookAngle>::iterator end() { return _sats.end(); }
  size_t size() const { return _sats.size(); }
  SatLookAngle &operator[](size_t index) {
    assert(index < size() && "Invalid index.");
    return _sats[index];
  }
};

// Queries the DB for TLE entries, and generates a container of TLEs and their
// look angles with respect to lat/lon/alt.
SatLookAngles getSatellitesAndLookAngles(double lat, double lon, double alt,
                                         DB &db);
#endif  // __SATNOW_MAIN_HH
