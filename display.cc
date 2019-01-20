#include "display.hh"
#include <SGP4.h>
#include <iomanip>
#include <iostream>
#include "main.hh"
#if HAVE_GUI
#include <menu.h>
#include <ncurses.h>
#endif

void DisplayConsole::render(SatLookAngles &TLEsAndLAs) {
  size_t count = 0;
  const size_t nTles = TLEsAndLAs.size();
  for (const auto &TL : TLEsAndLAs) {
    const auto &tle = TL.first;
    const auto &la = TL.second;
    std::cout << "[+] [" << (++count) << '/' << nTles << "] " << '('
              << tle.Name() << "): LookAngle: " << la << std::endl;
  }
}

void DisplayNCurses::render(SatLookAngles &sats) {
#if HAVE_GUI
  // Init ncurses.
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  // Build the menu and have a place to store the strings.
  auto items = new ITEM *[sats.size() + 1]();
  auto itemStrs = new std::string[sats.size()];
  items[sats.size()] = nullptr;

  // Column names.
  std::stringstream ss;
  ss << std::left << std::setw(10) << "ID" << std::setw(25) << "NAME"
     << std::setw(12) << "AZIMUTH" << std::setw(12) << "ELEVATION"
     << std::setw(12) << "RANGE (KM)";
  std::string colNames = ss.str();

  // Create a window to decorate the menu with.
  const int rows = std::min(LINES, 79);
  const int cols = std::min(COLS, 85);
  auto win = newwin(rows, cols, 0, 0);
  box(win, '|', '=');

  // Populate menu.
  MENU *menu = nullptr;
  auto updateMenu = [&](bool updatePositions) {
    if (updatePositions) {
      sats.updateTimeAndPositions();
      sats.sort();
    }

    // Remember cursor position so we can restore it.
    ITEM *curItem = current_item(menu);
    int curIdx = item_index(curItem);
    if (curIdx < 0) curIdx = 0;

    // Create the strings (items) for the menu.
    for (size_t i = 0; i < sats.size(); ++i) {
      const auto &tle = sats[i].first;
      const auto &la = sats[i].second;
      std::stringstream ss;
      ss << std::left << std::setw(10) << std::to_string(i) << std::setw(25)
         << tle.Name() << std::setw(12) << std::to_string(la.azimuth)
         << std::setw(12) << std::to_string(la.elevation) << std::setw(12)
         << std::to_string(la.range);
      itemStrs[i] = ss.str();
      if (items[i]) free_item(items[i]);
      items[i] = new_item(itemStrs[i].c_str(), nullptr);

      if (i == (size_t)curIdx)
        curItem = items[i];  // Updated item at cursor position.
    }

    // Rebuild the menu (freeing the previous one).
    if (menu) {
      unpost_menu(menu);
      free_menu(menu);
    }
    menu = new_menu(items);
    set_menu_mark(menu, "->");
    set_menu_format(menu, std::min(sats.size(), (size_t)rows - 5), 1);
    set_menu_win(menu, win);
    set_menu_sub(menu, derwin(win, rows - 5, cols - 2, 2, 1));
    set_current_item(menu, curItem);
    post_menu(menu);
  };

  updateMenu(false);  // 'false' avoids calculating new look angles.

  // Add title and column names.
  mvwprintw(win, 0, (cols / 2 - 12), "%s", "}-- satnow " VER " --{");
  mvwprintw(win, 1, 3, "%s", colNames.c_str());

  // Print a legend at the bottom.
  mvwhline(win, rows - 3, 1, '-', cols - 2);
  mvwprintw(win, rows - 2, 1, "%s",
            "[Quit: (q)] [Update: (space)] "
            "[Movement: (pg)up/(pg)down] [Details: (enter)]");

  // Display.
  refresh();
  wrefresh(win);
  const int msecs = (_refreshSecs < 0) ? -1 : _refreshSecs;
  timeout(msecs);
  int c;
  while ((c = getch()) != 'q') {
    switch (c) {
      case KEY_DOWN:
        menu_driver(menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver(menu, REQ_UP_ITEM);
        break;
      case KEY_NPAGE:
        menu_driver(menu, REQ_SCR_DPAGE);
        break;
      case KEY_PPAGE:
        menu_driver(menu, REQ_SCR_UPAGE);
        break;
      case ' ':
      case ERR:
        updateMenu(true);
        refresh();
        break;
    }
    wrefresh(win);
  }

  // Cleanup.
  for (size_t i = 0; i < sats.size() + 1; ++i) free_item(items[i]);
  delete[] items;
  delete[] itemStrs;
  delwin(win);
  endwin();
#endif  // HAVE_GUI
}
