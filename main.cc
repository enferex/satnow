#include <curl/curl.h>
#include <getopt.h>
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <string>
#include <vector>
#if HAVE_GUI
#include <ncurses.h>
#include <menu.h>
#endif

#include <CoordTopocentric.h>
#include <Observer.h>
#include <SGP4.h>

// Resources:
// https://www.celestrak.com/NORAD/documentation/tle-fmt.php
// https://en.wikipedia.org/wiki/Two-line_element_set

#define DEFAULT_DB_PATH "./.satnow.sql3"

using SatLookAngle = std::pair<Tle, CoordTopocentric>;

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
  std::cout << "Usage: " << execname
            << " --lat=val --lon=val "
            << "[--help --verbose --alt=val --update=file --db=file "
#if HAVE_GUI
            << " --gui"
#endif
            << ']' << std::endl
            << "  --lat=<latitude in degrees>" << std::endl
            << "  --lon=<longitude in degrees>" << std::endl
            << "  --alt=<altitude in meters>" << std::endl
            << "  --db=<path to database> (default: " << DEFAULT_DB_PATH << ')'
            << std::endl
            << "  --help/-h:    This help message." << std::endl
            << "  --verbose/-v: Output additional data (for debugging)." << std::endl
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
    if (en != std::string::npos)
      name = name.substr(0, en+1);
    // Celestrak and wikipedia say that names are 24 bytes, libsgp4 says 22.
    if (name.size() > 22)
      name = name.substr(0, 22);
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
static void update(const char *sourceFile, sqlite3 *sql, bool verbose) {
  assert(sourceFile && sql && "Invalid input to update.");

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
    std::string q = "INSERT OR REPLACE INTO tle (name, norad, line1, line2) ";
    q += "VALUES (\'" + tle.Name() + "\', " + std::to_string(tle.NoradNumber()) +
         ", \"" + tle.Line1() + "\", \"" + tle.Line2() + "\");";
    const int err = sqlite3_exec(sql, q.c_str(), nullptr, nullptr, nullptr);
    if (verbose)
      std::cout << "[+] Refreshing [" << (++count) << '/' << results.size()
                << "]: " << std::to_string(tle.NoradNumber()) << " ("
                << tle.Name() << ") [" << (!err ? "Good" : "Failed") << ']'
                << std::endl;
  }
}

static std::vector<Tle> fetchTLEs(sqlite3 *sql) {
  std::vector<Tle> tles;
  const char *q = "SELECT name, line1, line2 FROM tle;";
  auto cb = [](void *tleptr, int nCols, char **row, char **colName) {
    auto tles = static_cast<std::vector<Tle>*>(tleptr);
    if (nCols != 3) return SQLITE_OK;
    std::string name(row[0]);
    std::string line1(row[1]);
    std::string line2(row[2]);
    if (name.size())
      tles->emplace_back(name, line1, line2);
    else
      tles->emplace_back(name, line1, line2);
    return SQLITE_OK;
  };

  if (sqlite3_exec(sql, q, cb, &tles, nullptr)) {
    std::cerr << "[-] Error querying database: " << sqlite3_errmsg(sql) << std::endl;
    return tles;
  }

  return tles;
}

static sqlite3 *initDatabase(const char *dbFile) {
  sqlite3 *sql;
  if (sqlite3_open(dbFile, &sql)) return sql;

  const char *q =
      "CREATE TABLE IF NOT EXISTS tle "
      "(timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
      "norad INT PRIMARY KEY, "
      "name TEXT, line1 TEXT, line2 TEXT)";

  if (sqlite3_exec(sql, q, nullptr, nullptr, nullptr)) return sql;

  return sql;
}

static void displayResults(std::vector<SatLookAngle> &TLEsAndLAs) {
  size_t count = 0;
  const size_t nTles = TLEsAndLAs.size();
  for (const auto &TL : TLEsAndLAs) {
    const auto &tle = TL.first;
    const auto &la = TL.second;
    std::cout << "[+] [" << (++count) << '/' << nTles << "] "
              << '(' << tle.Name() << "): LookAngle: " << la << std::endl;
  }
}

static std::vector<SatLookAngle> getSatellitesAndLocations(
    double lat, double lon, double alt, sqlite3 *sql) {
  // Get the TLEs.
  std::vector<Tle> tles = fetchTLEs(sql);

  // Build the observer (user's) position.
  auto me = Observer(lat, lon, alt);

  // Use the current datetime.
  auto now = DateTime::Now(true);

  // Compute and report the look angles.
  std::vector<SatLookAngle> TLEsAndLAs;
  for (const auto &tle : tles) {
    const auto model = SGP4(tle);
    const auto pos = model.FindPosition(now);
    const auto la = me.GetLookAngle(pos);
    TLEsAndLAs.emplace_back(tle, la);
  }

  // Sort by increasing range.
  std::sort(TLEsAndLAs.begin(), TLEsAndLAs.end(), [](const SatLookAngle &a,
                                                     const SatLookAngle &b) {
      return a.second.range < b.second.range; });

  return TLEsAndLAs;
}

static void runGUI(std::vector<SatLookAngle> &TLEsAndLAs) {
#if HAVE_GUI
  // Init curses.
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  // Build the menu and have a place to store the strings (row_xx arrays).
  auto items = new ITEM*[TLEsAndLAs.size() * 5 + 2];
  items[TLEsAndLAs.size()*5] = nullptr;
  auto rowId = new std::string[TLEsAndLAs.size()];
  auto rowName = new std::string[TLEsAndLAs.size()];
  auto rowAz = new std::string[TLEsAndLAs.size()];
  auto rowElev = new std::string[TLEsAndLAs.size()];
  auto rowRange = new std::string[TLEsAndLAs.size()];

  items[0] = new_item("ID", nullptr);
  items[1] = new_item("NAME", nullptr);
  items[2] = new_item("AZIMUTH", nullptr);
  items[3] = new_item("ELEVATION", nullptr);
  items[4] = new_item("RANGE (KM)", nullptr);
  for (size_t i=1; i<TLEsAndLAs.size(); ++i) {
    const auto &tle = TLEsAndLAs[i].first;
    const auto &la = TLEsAndLAs[i].second;
    rowId[i] = std::to_string(i);
    rowName[i] = tle.Name();
    rowAz[i] = std::to_string(la.azimuth);
    rowElev[i] = std::to_string(la.elevation);
    rowRange[i] = std::to_string(la.range);
    items[(i*5)]   = new_item(rowId[i].c_str(),    nullptr);
    items[(i*5)+1] = new_item(rowName[i].c_str(),  nullptr);
    items[(i*5)+2] = new_item(rowAz[i].c_str(),    nullptr);
    items[(i*5)+3] = new_item(rowElev[i].c_str(),   nullptr);
    items[(i*5)+4] = new_item(rowRange[i].c_str(), nullptr);
  }
  auto menu = new_menu(items);

  // Setup the menu format (columns).
  set_menu_mark(menu, "->");
  set_menu_format(menu, TLEsAndLAs.size(), 5); // Number, Name, Az, Ele, Range.

  // Create a window to decorate the menu with.
  auto win = newwin(70, 110, 2, 2);
  set_menu_win(menu, win);
  set_menu_sub(menu, derwin(win, 60, 100, 2, 2));
  box(win, '|', '=');

  // Display.
  refresh();
  post_menu(menu);
  wrefresh(win);
  wrefresh(win);
  int c;
  while ((c = getch()) != 'q') {
    switch (c) {
      case KEY_DOWN: menu_driver(menu, REQ_DOWN_ITEM); break;
      case KEY_UP: menu_driver(menu, REQ_UP_ITEM); break;
    }
    refresh();
    wrefresh(win);
  }

  // Cleanup.
  for (size_t i=0; i<TLEsAndLAs.size()*5+1; ++i)
    free_item(items[i]);
  delete [] items;
  delete [] rowId;
  delete [] rowName;
  delete [] rowAz;
  delete [] rowElev;
  delete [] rowRange;
  free_menu(menu);
  endwin();
#endif // HAVE_GUI
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
  sqlite3 *sql = initDatabase(dbFile);
  if (sqlite3_errcode(sql)) {
    std::cerr << "[-] Error opening database '" << dbFile
              << "': " << sqlite3_errmsg(sql) << std::endl;
    return EXIT_FAILURE;
  }

  // If a source file is specified, then update the existing database.
  if (sourceFile) update(sourceFile, sql, verbose);

  // Calculate and display.
  auto TLEsAndLAs = getSatellitesAndLocations(lat, lon, alt, sql);
  if (gui)
    runGUI(TLEsAndLAs);
  else
    displayResults(TLEsAndLAs);

  // Cleanup.
  sqlite3_close(sql);
  return 0;
}
