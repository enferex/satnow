#include "db.hh"
#define VER "0.1b"

std::vector<Tle> DBSQLite::fetchTLEs() {
  std::vector<Tle> tles;
  const char *q = "SELECT name, line1, line2 FROM tle;";
  auto cb = [](void *tleptr, int nCols, char **row, char **colName) {
    auto tles = static_cast<std::vector<Tle> *>(tleptr);
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

  if (sqlite3_exec(_sql, q, cb, &tles, nullptr)) {
    std::cerr << "[-] Error querying database: " << sqlite3_errmsg(_sql)
              << std::endl;
    return tles;
  }

  return tles;
}

void DBSQLite::update(const Tle &tle) {
  std::string q = "INSERT OR REPLACE INTO tle (name, norad, line1, line2) ";
  q += "VALUES (\'" + tle.Name() + "\', " + std::to_string(tle.NoradNumber()) +
       ", \"" + tle.Line1() + "\", \"" + tle.Line2() + "\");";
  sqlite3_exec(_sql, q.c_str(), nullptr, nullptr, nullptr);
}

DBSQLite::DBSQLite(const char *dbFile) {
  // Open DB and if not a failure, then setup the table data.
  if (!sqlite3_open(dbFile, &_sql)) {
    const char *q =
        "CREATE TABLE IF NOT EXISTS tle "
        "(timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "norad INT PRIMARY KEY, "
        "name TEXT, line1 TEXT, line2 TEXT)";
    sqlite3_exec(_sql, q, nullptr, nullptr, nullptr);
  }
}

DBSQLite::~DBSQLite() { sqlite3_close(_sql); }

bool DBSQLite::ok() const { return sqlite3_errcode(_sql) == SQLITE_OK; }

std::string DBSQLite::getErrorString() const { return sqlite3_errmsg(_sql); }
