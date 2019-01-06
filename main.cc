#include <getopt.h>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <CoordTopocentric.h>
#include <Observer.h>
#include <SGP4.h>

#define DEFAULT_DB_PATH "./.satnow.sql3"

// Command line options.
static const struct option opts[] = {
    {"alt", required_argument, nullptr, 'a'},
    {"lat", required_argument, nullptr, 'x'},
    {"lon", required_argument, nullptr, 'y'},
    {"update", required_argument, nullptr, 'u'},
    {"db", required_argument, nullptr, 'd'}};

[[noreturn]] static void usage(const char *execname) {
  std::cout << "Usage: " << execname
            << " [file.tle ...] [--lat=val --lon=val --alt=val --update=file "
               "--db=file]"
            << std::endl
            << "  --lat=<latitude in degrees>" << std::endl
            << "  --lon=<longitude in degrees>" << std::endl
            << "  --alt=<altitude in meters>" << std::endl
            << "  --db=<path to database> (default: " << DEFAULT_DB_PATH << ')'
            << std::endl
            << "  --update=<sources>  " << std::endl
            << "    Where 'sources' is a text file containing a " << std::endl
            << "    list of URLs that point to a TLE .txt file. " << std::endl
            << "    Each source must be on its own line in the file."
            << std::endl;
  exit(EXIT_SUCCESS);
}

// Return a vector of TLE instances for each TLE entry.
// This supports both forms of TLE where each line of data (two of them) are 69
// bytes each, and the optional name line is 22 bytes.
static std::vector<Tle> readTLEs(const char *fname) {
  std::vector<Tle> tles;
  std::string line1, line2, name;
  std::ifstream fh(fname);
  size_t lineNo = 0;

  while (std::getline(fh, line1)) {
    ++lineNo;
    if (std::isalpha(line1[0])) {
      name = line1;
      ++lineNo;
      if (!std::getline(fh, line1)) {
        std::cerr << "Unexpected error reading TLE line 1 at line " << lineNo
                  << " in " << fname << std::endl;
        return tles;
      }
    }
    ++lineNo;
    if (!std::getline(fh, line2)) {
      std::cerr << "Unexpected error reading TLE line 2 at line " << lineNo
                << " in " << fname << std::endl;
      return tles;
    }

    tles.emplace_back(name.substr(0, 22), line1.substr(0, 69),
                      line2.substr(0, 69));
    name.clear();
  }

  return tles;
}

static bool tryParseFile(const std::string &str) {
  // If it looks like a URL, don't try to open it.
  if (str.find("://") != std::string::npos) return false;

  // Try to open the thing.
  ifstream fh(str);
  if (!fh.is_open() return false;

  return false;
}

static bool tryParseURL(const std::string &str) { return false; }

// Update the database of TLEs.
static void update(const char *sourceFile, const char *dbPath) {
  // Open database.
  // TODO

  // Parse the source file (one entry per line).
  std::string line;
  std::ifstream fh(sourceFile);
  size_t lineNumber = 0;
  while (std::getline(fh, line)) {
    ++lineNumber;
    // Trim white space from both ends.
    size_t st, en;
    if ((st = line.find_first_not_of(" \t\r\n")) == std::string::npos ||
        (en = line.find_last_not_of("# \t\r\n")) == std::string::npos)
      continue;

    // If a comment line, ignore.
    if (line[st] == '#') continue;

    // Reject empty lines.
    if (en - st == 0) continue;

    // Trim comments and any spaces from the end.
    auto comments = line.find_first_of("# \t\r\n", st);
    if (comments != std::string::npos) en = comments;

    // Get the trimmed string.
    auto str = line.substr(st, en - st);

    // Parse the contents at the url or in the file.
    if (!tryParseFile(str) && !tryParseURL(str)) {
      std::cerr << "[-] Unknown entry in " << sourceFile << " Line "
                << lineNumber << std::endl;
      continue;
    }
  }
}

int main(int argc, char **argv) {
  // Handle arguments.
  double alt = 0.0, lat = 0.0, lon = 0.0;
  const char *sourceFile = nullptr, *dbFile = DEFAULT_DB_PATH;
  int opt;
  while ((opt = getopt_long(argc, argv, "ha:x:y:u:d:", opts, nullptr)) > 0) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        break;
      case 'a':
        alt = std::stod(optarg);
        break;
      case 'x':
        lat = std::stod(optarg);
        break;
      case 'y':
        lon = std::stod(optarg);
        break;
      case 'u':
        sourceFile = optarg;
        break;
      case 'd':
        dbFile = optarg;
        break;
      default: {
        std::cerr << "[-] Unknown command line option." << std::endl;
        exit(EXIT_FAILURE);
        break;
      }
    }
  }

  // If a source file is specified, then update the existing database.
  update(sourceFile, dbFile);

  // Build the observer (user's position)
  auto me = Observer(37.584, -122.366, 0.0);

  // Use the current datetime.
  auto now = DateTime(2018, 1, 1, 1, 1, 0);

  // The positional arguments are the TLE source files or URLs.
  for (int i = optind; i < argc; ++i) {
    const char *fname = argv[i];
    auto tles = readTLEs(fname);
    for (const auto &tle : tles) {
      const auto model = SGP4(tle);
      const auto pos = model.FindPosition(now);
      const auto la = me.GetLookAngle(pos);
      std::cout << "Look angle: (" << tle.Name() << ") " << la << std::endl;
    }
  }

  return 0;
}
