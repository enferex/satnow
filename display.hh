// satnow: display.hh
//
// Copyright 2019 Matt Davis (https://github.com/enferex) 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  DisplayNCurses(int refreshSeconds = -1);
  virtual ~DisplayNCurses();
  void render(SatLookAngles &sats) override final;
};

#endif  // __SATNOW_DISPLAY_HH
