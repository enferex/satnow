#ifndef __SATNOW_DB_HH
#define __SATNOW_DB_HH
#include <Tle.h>
#include <sqlite3.h>
#include <string>
#include <vector>

#define DEFAULT_DB_PATH "./.satnow.sql3"

class DB {
 public:
  virtual std::vector<Tle> fetchTLEs() = 0;
  virtual void update(const Tle &tle) = 0;
  virtual bool ok() const = 0;
  virtual std::string getErrorString() const = 0;
};

class DBSQLite final : public DB {
 private:
  sqlite3 *_sql;

 public:
  DBSQLite(const char *dbFile);
  virtual ~DBSQLite();
  bool ok() const override final;
  void update(const Tle &tle) override final;
  std::vector<Tle> fetchTLEs() override final;
  std::string getErrorString() const override final;
};

#endif  // __SATNOW_DB_HH
