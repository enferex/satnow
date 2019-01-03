#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <vector>
#include <cctype>
#include <getopt.h>

#include <CoordTopocentric.h>
#include <Observer.h>
#include <SGP4.h>

[[ noreturn ]] static void usage(const char *execname) {
  std::cout << "Usage: " << execname
            << " [file.tle ...] [--lat=val --lon=val --alt=val]"
            << std::endl
            << "  --lat=<latitude in degrees>" << std::endl
            << "  --lon=<longitude in degrees>" << std::endl
            << "  --alt=<altitude in meters>" << std::endl;
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
        std::cerr << "Unexpected error reading TLE line 1 at line " << lineNo << " in " << fname << std::endl;
        return tles;
      }
    }
    ++lineNo;
    if (!std::getline(fh, line2)) {
      std::cerr << "Unexpected error reading TLE line 2 at line " << lineNo << " in " << fname << std::endl;
      return tles;
    }

    tles.emplace_back(name.substr(0,22), line1.substr(0,69), line2.substr(0,69));
    name.clear();
  }

  return tles;
}

static const struct option opts[] = {
  {"alt", required_argument, nullptr, 'a'},
  {"lat", required_argument, nullptr, 'x'},
  {"lon", required_argument, nullptr, 'y'}
};

int main(int argc, char **argv) {
  // Handle arguments.
  double alt = 0.0, lat = 0.0, lon = 0.0;
  int opt;
  while ((opt = getopt_long(argc, argv, "ha:x:y:", opts, nullptr)) > 0) {
    switch (opt) {
      case 'h': usage(argv[0]); break;
      case 'a': alt = std::stod(optarg); break;
      case 'x': lat = std::stod(optarg); break;
      case 'y': lon = std::stod(optarg); break;
      default: {
         std::cerr <<  "[-] Unknown command line option." << std::endl;
         exit(EXIT_FAILURE);
         break;
      }
    }
  }

  // Build the observer (user's position)  
  auto me = Observer(37.584, -122.366, 0.0);

  // Use the current datetime.
  auto now = DateTime(2018, 1, 1, 1, 1, 0);

  // The positional arguments are the TLE source files or URLs.
  for (int i=optind; i<argc; ++i) {
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
