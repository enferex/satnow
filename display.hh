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
 public:
  void render(SatLookAngles &sats) override final;
};

#endif  // __SATNOW_DISPLAY_HH
