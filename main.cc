#include <curl/curl.h>
#include <getopt.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#if HAVE_GUI
#include <menu.h>
#include <ncurses.h>
#endif
#include <CoordTopocentric.h>
#include <Observer.h>
#include <SGP4.h>
#include "db.hh"

// Resources:
// https://www.celestrak.com/NORAD/documentation/tle-fmt.php
// https://en.wikipedia.org/wiki/Two-line_element_set

#define VER "0.1b"

using SatLookAngle = std::pair<Tle, CoordTopocentric>;

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

// Command line options.
static const struct option opts[] = {
    {"alt", required_argument, nullptr, 'a'},
    {"lat", required_argument, nullptr, 'x'},
    {"lon", required_argument, nullptr, 'y'},
    {"update", required_argument, nullptr, 'u'},
    {"db", required_argument, nullptr, 'd'},
    {"verbose", no_argument, nullptr, 'v'},
#if HAVE_GUI
    {"gui", no_argument, nullptr, 'g'},
#endif
    {"help", no_argument, nullptr, 'h'}};

[[noreturn]] static void usage(const char *execname) {
  std::cout << "satnow v" << VER << std::endl
            << "Usage: " << execname << " --lat=val --lon=val "
            << "[-h -v --alt=val --update=file --db=file "
#if HAVE_GUI
            << "--gui"
#endif
            << ']' << std::endl
            << "  --lat=<latitude in degrees>" << std::endl
            << "  --lon=<longitude in degrees>" << std::endl
            << "  --alt=<altitude in meters>" << std::endl
            << "  --db=<path to database> (default: " << DEFAULT_DB_PATH << ')'
            << std::endl
            << "  --help/-h:    This help message." << std::endl
            << "  --verbose/-v: Output additional data (for debugging)."
            << std::endl
#if HAVE_GUI
            << "  --gui: Enable curses/gui mode." << std::endl
#endif
            << "  --update=<sources>  " << std::endl
            << "    Where 'sources' is a text file containing a " << std::endl
            << "    list of URLs that point to a TLE .txt file. " << std::endl
            << "    Each source must be on its own line in the file."
            << std::endl;
  exit(EXIT_SUCCESS);
}

bool readLine(FILE *fp, std::string &str) {
  if (feof(fp) || ferror(fp)) return false;
  char *line;
  size_t n = 0;
  if (!getline(&line, &n, fp) || n == 0) return false;
  str = std::string(line);
  free(line);
  if (str.size() == 0 && feof(fp))
    return false;
  else
    return true;
}

// Return a vector of TLE instances for each TLE entry.
// This supports both forms of TLE where each line of data (two of them) are 69
// bytes each, and the optional name line is 24 bytes.
static std::vector<Tle> readTLEs(const std::string &fname, FILE *fp) {
  std::vector<Tle> tles;
  std::string line1, line2, name;
  size_t lineNo = 0;

  while (readLine(fp, line1)) {
    ++lineNo;
    if (std::isalpha(line1[0]) || line1.size() <= 24) name = line1;
    // Trim trailing whitespace from name.
    auto en = name.find_last_not_of(" \t\r\n");
    if (en != std::string::npos) name = name.substr(0, en + 1);
    // Celestrak and wikipedia say that names are 24 bytes, libsgp4 says 22.
    if (name.size() > 22) name = name.substr(0, 22);
    ++lineNo;
    if (!readLine(fp, line1)) {
      std::cerr << "Unexpected error reading TLE line 1 at line " << lineNo
                << " in " << fname << std::endl;
      return tles;
    }
    ++lineNo;
    if (!readLine(fp, line2)) {
      std::cerr << "Unexpected error reading TLE line 2 at line " << lineNo
                << " in " << fname << std::endl;
      return tles;
    }

    tles.emplace_back(name, line1.substr(0, 69), line2.substr(0, 69));
    name.clear();
  }

  return tles;
}

static bool tryParseFile(const std::string &fname, std::vector<Tle> &tles) {
  // If it looks like a URL, don't try to open it.
  if (fname.find("://") != std::string::npos) return false;

  FILE *fp;  // We use c-style file i/o so we can use tmpfile in tryParseURL.
  if (!(fp = fopen(fname.c_str(), "r"))) return false;

  // Read the new TLEs and add them to 'tles'.
  auto newTLEs = readTLEs(fname, fp);
  tles.insert(tles.end(), std::make_move_iterator(newTLEs.begin()),
              std::make_move_iterator(newTLEs.end()));
  fclose(fp);
  return true;
}

static bool tryParseURL(const std::string &fname, std::vector<Tle> &tles) {
  CURL *crl = curl_easy_init();
  if (!crl) return false;
  if (curl_easy_setopt(crl, CURLOPT_URL, fname.c_str())) return false;

  // Ignore any "URLs" that do not have a protocol delimiter.
  if (fname.find("://") == std::string::npos) return false;

  // Prepare to download the TLE data into a temp file.
  FILE *fp = tmpfile();
  if (!fp) return false;
  if (curl_easy_setopt(crl, CURLOPT_WRITEDATA, fp)) {
    fclose(fp);
    curl_easy_cleanup(crl);
    return false;
  }

  // Download the data.
  std::cout << "[+] Downloading contents from " << fname << std::endl;
  bool ok = !!curl_easy_perform(crl);
  curl_easy_cleanup(crl);

  // Read the new TLEs and add them to 'tles'.
  rewind(fp);
  auto newTLEs = readTLEs(fname, fp);
  tles.insert(tles.end(), std::make_move_iterator(newTLEs.begin()),
              std::make_move_iterator(newTLEs.end()));
  fclose(fp);
  return ok;
}

// Update the database of TLEs.
static void update(const char *sourceFile, DB &db, bool verbose) {
  assert(sourceFile && db.ok() && "Invalid input to update.");

  // Parse the source file (one entry per line).
  std::string line;
  std::ifstream fh(sourceFile);
  size_t lineNumber = 0;
  std::vector<Tle> results;
  while (std::getline(fh, line)) {
    ++lineNumber;
    // Trim white space from both ends.
    size_t st, en;
    if ((st = line.find_first_not_of(" \t\r\n")) == std::string::npos ||
        (en = line.find_last_not_of("# \t\r\n")) == std::string::npos)
      continue;

    ++en;

    // If a comment line, ignore.
    if (line[st] == '#') continue;

    // Reject empty lines.
    if (en - st == 0) continue;

    // Trim comments and any spaces from the end.
    const auto comments = line.find_first_of("# \t\r\n", st);
    if (comments != std::string::npos) en = comments;

    // Get the trimmed string.
    const auto str = line.substr(st, en - st);

    // Parse the contents at the url or in the file.
    std::cerr << "[+] Loading TLEs from '" << str << '\'' << std::endl;
    if (!tryParseFile(str, results) && !tryParseURL(str, results)) {
      std::cerr << "[-] Unknown entry in " << sourceFile << " Line "
                << lineNumber << std::endl;
      continue;
    }
  }

  // Update the database.
  size_t count = 0;
  for (const auto &tle : results) {
    db.update(tle);
    if (verbose)
      std::cout << "[+] Refreshing [" << (++count) << '/' << results.size()
                << "]: " << std::to_string(tle.NoradNumber()) << " ("
                << tle.Name() << ") [" << (db.ok() ? "Good" : "Failed") << ']'
                << std::endl;
  }
}

static void displayResults(SatLookAngles &TLEsAndLAs) {
  size_t count = 0;
  const size_t nTles = TLEsAndLAs.size();
  for (const auto &TL : TLEsAndLAs) {
    const auto &tle = TL.first;
    const auto &la = TL.second;
    std::cout << "[+] [" << (++count) << '/' << nTles << "] " << '('
              << tle.Name() << "): LookAngle: " << la << std::endl;
  }
}

static SatLookAngles getSatellitesAndLookAngles(double lat,
                                                double lon,
                                                double alt, DB &db) {
  SatLookAngles sats(lat, lon, alt);

  // Get the TLEs.
  std::vector<Tle> tles = db.fetchTLEs();

  // Add the TLEs (this will automatically generate look angles.).
  for (const auto &tle : tles)
    sats.add(tle);

  // Sort by increasing range.
  sats.sort();
  return sats;
}

static void runGUI(SatLookAngles &sats) {
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

  // Draw the text help legend.

  // Populate menu.
  MENU *menu = nullptr;
  auto updateMenu = [&](bool updatePositions) {
    if (updatePositions) {
      sats.updateTimeAndPositions();
      sats.sort();
    }

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
    }

    // Rebuild the menu (freeing the previous one).
    if (menu) { 
      unpost_menu(menu);
      free_menu(menu);
    }
    menu = new_menu(items);
    set_menu_mark(menu, "->");
    set_menu_format(menu, std::min(sats.size(), (size_t)rows - 4), 1);
    set_menu_win(menu, win);
    set_menu_sub(menu, derwin(win, rows - 4, cols - 2, 2, 1));
    post_menu(menu);
  };

  updateMenu(false); // 'false' avoids calculating new look angles.

  // Add title and column names.
  mvwprintw(win, 0, (cols / 2 - 9), "%s", "}-- satnow " VER " --{");
  mvwprintw(win, 1, 3, "%s", colNames.c_str());

  // Print a legend at the bottom.
  mvwhline(win, rows-3, 1, '-', cols-2);
  mvwprintw(win, rows-2, 1, "%s",
            "[Quit: (q)] [Update: (space)] "
            "[Movement: (pg)up/(pg)down] [Details: (enter)]");

  // Display.
  refresh();
  wrefresh(win);
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

int main(int argc, char **argv) {
  double alt = 0.0, lat = 0.0, lon = 0.0;
  bool verbose = false, gui = false;
  const char *sourceFile = nullptr, *dbFile = DEFAULT_DB_PATH;
  int opt;
  while ((opt = getopt_long(argc, argv, "ghva:d:u:x:y:", opts, nullptr)) > 0) {
    switch (opt) {
#if HAVE_GUI
      case 'g':
        gui = true;
        break;
#endif
      case 'h':
        usage(argv[0]);
        break;
      case 'v':
        verbose = true;
        break;
      case 'a':
        alt = std::stod(optarg);
        break;
      case 'd':
        dbFile = optarg;
        break;
      case 'u':
        sourceFile = optarg;
        break;
      case 'x':
        lat = std::stod(optarg);
        break;
      case 'y':
        lon = std::stod(optarg);
        break;
      default:
        std::cerr << "[-] Unknown command line option." << std::endl;
        exit(EXIT_FAILURE);
        break;
    }
  }

  // Ensure we have valid coords.
  if (lat > 90.0 || lat < -90.0 || lon > 180.0 || lon < -180.0) {
    std::cerr << "[-] Invalid coordinates (latitude: " << lat
              << ", longitude: " << lon << ')' << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "[+] Using viewer position (latitude: " << lat
            << ", longitude: " << lon << ", "
            << ", altitude: " << alt << ')' << std::endl;

  // Open the database that contains the TLE data.
  if (!dbFile) {
    std::cerr << "[-] The database path must not be empty (see --help)."
              << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "[+] Using database: " << dbFile << std::endl;

  DBSQLite db(dbFile);
  if (!db.ok()) {
    std::cerr << "[-] Error opening database '" << dbFile
              << "': " << db.getErrorString() << std::endl;
    return EXIT_FAILURE;
  }

  // If a source file is specified, then update the existing database.
  if (sourceFile) update(sourceFile, db, verbose);

  // Calculate and display.
  auto TLEsAndLAs = getSatellitesAndLookAngles(lat, lon, alt, db);
  if (gui)
    runGUI(TLEsAndLAs);
  else
    displayResults(TLEsAndLAs);

  return 0;
}
