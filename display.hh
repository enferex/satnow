#ifndef __SATNOW_DISPLAY_HH
#define __SATNOW_DISPLAY_HH
#include <sstream>
#include <utility>
#if HAVE_GUI
#include <menu.h>
#include <ncurses.h>
#endif

class SatLookAngles;

class Display {
 public:
  virtual void render(SatLookAngles &sats) = 0;
};

class DisplayConsole final : public Display {
 public:
  void render(SatLookAngles &sats) override final;
};

class DisplayNCurses final : public Display {
 private:
  int _refreshSecs;  // Number of seconds between refreshing gui data.
 public:
  DisplayNCurses(int refreshSeconds = -1) : _refreshSecs(refreshSeconds) {}
  void render(SatLookAngles &sats) override final;
};

#endif  // __SATNOW_DISPLAY_HH
