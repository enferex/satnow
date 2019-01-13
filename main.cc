#include <getopt.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <sqlite3.h>

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
    {"db", required_argument, nullptr, 'd'},
    {"help", no_argument, nullptr, 'h'}};

[[noreturn]] static void usage(const char *execname) {
  std::cout << "Usage: " << execname
            << " --lat=val --lon=val [--alt=val --update=file --db=file]"
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

bool readLine(FILE *fp, std::string &str) {
  if (feof(fp) || ferror(fp))
    return false;
  char *line;
  size_t n = 0;
  if (!getline(&line, &n, fp) || n == 0)
    return false;
  str = std::string(line);
  free(line);
  if (str.size() == 0 && feof(fp)) return false;
  else return true;
}

// Return a vector of TLE instances for each TLE entry.
// This supports both forms of TLE where each line of data (two of them) are 69
// bytes each, and the optional name line is 22 bytes.
static std::vector<Tle> readTLEs(const std::string &fname, FILE *fp) {
  std::vector<Tle> tles;
  std::string line1, line2, name;
  size_t lineNo = 0;

  while (readLine(fp, line1)) {
    ++lineNo;
    if (std::isalpha(line1[0])) {
      name = line1;
      // Trim trailing whitespace from name.
      for (int i=name.size()-1; i>=0; --i)
        if (isspace(name[i]))
          name[i] = '\0';
      ++lineNo;
      if (!readLine(fp, line1)) {
        std::cerr << "Unexpected error reading TLE line 1 at line " << lineNo
                  << " in " << fname << std::endl;
        return tles;
      }
    }
    ++lineNo;
    if (!readLine(fp, line2)) {
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

static bool tryParseFile(const std::string &fname, std::vector<Tle> &tles) {
  // If it looks like a URL, don't try to open it.
  if (fname.find("://") != std::string::npos) return false;

  FILE *fp;  // We use c-style file i/o so we can use tmpfile in tryParseURL.
  if (!(fp = fopen(fname.c_str(), "r"))) return false;

  // Read the new TLEs and add them to 'tles'.
  auto newTLEs = readTLEs(fname, fp);
  tles.insert(tles.end(),
              std::make_move_iterator(newTLEs.begin()),
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
  tles.insert(tles.end(),
              std::make_move_iterator(newTLEs.begin()),
              std::make_move_iterator(newTLEs.end()));
  fclose(fp);
  return ok;
}

// Update the database of TLEs.
static void update(const char *sourceFile, sqlite3 *sql) {
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

    // If a comment line, ignore.
    if (line[st] == '#') continue;

    // Reject empty lines.
    if (en - st == 0) continue;

    // Trim comments and any spaces from the end.
    auto comments = line.find_first_of("# \t\r\n", st);
    if (comments != std::string::npos) en = comments;

    // Get the trimmed string.
    auto str = line.substr(st, en - st + 1);

    // Parse the contents at the url or in the file.
    if (!tryParseFile(str, results) && !tryParseURL(str, results)) {
      std::cerr << "[-] Unknown entry in " << sourceFile << " Line "
                << lineNumber << std::endl;
      continue;
    }
  }

  // Update the database.
  size_t count = 0;
  for (const auto &tle : results) {
    std::cout << "[+] Refreshing ["
              << (++count) << '/' << results.size()  << "]: "
              << tle.Name() << std::endl;
  }
}

static std::vector<Tle> fetchTLEs(sqlite3 *sql) {
  std::vector<Tle> tles;
  return tles;
}

static sqlite3 *initDatabase(const char *dbFile) {
  sqlite3 *sql;
  if (sqlite3_open(dbFile, &sql))
    return sql;

  const char *q = "CREATE TABLE IF NOT EXISTS tle "
                  "(timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                  "name PRIMARY_KEY TEXT, line1 TEXT, line2 TEXT)";

  if (sqlite3_exec(sql, q, nullptr, nullptr, nullptr))
    return sql;

  return sql;
}

int main(int argc, char **argv) {
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

  // Open the database that contains the TLE data.
  if (!dbFile) {
    std::cerr << "[-] The database path must not be empty (see --help)." << std::endl;
    return EXIT_FAILURE;
  }
  sqlite3 *sql = initDatabase(dbFile);
  if (sqlite3_errcode(sql)) {
    std::cerr << "Error opening database '" << dbFile << "': "
              << sqlite3_errmsg(sql)
              << std::endl;
    return EXIT_FAILURE;
  }

  // If a source file is specified, then update the existing database.
  if (sourceFile)
    update(sourceFile, sql);

  // Get the TLEs.
  std::vector<Tle> tles = fetchTLEs(sql);

  // Build the observer (user's position)
  auto me = Observer(37.584, -122.366, 0.0);

  // Use the current datetime.
  auto now = DateTime(2018, 1, 1, 1, 1, 0);

  for (int i = optind; i < argc; ++i) {
    for (const auto &tle : tles) {
      const auto model = SGP4(tle);
      const auto pos = model.FindPosition(now);
      const auto la = me.GetLookAngle(pos);
      std::cout << "Look angle: (" << tle.Name() << ") " << la << std::endl;
    }
  }

  // Cleanup.
  sqlite3_close(sql);
  return 0;
}
