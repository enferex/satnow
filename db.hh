// satnow: main.cc
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
