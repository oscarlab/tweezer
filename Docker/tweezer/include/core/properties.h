//
//  properties.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/9/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_PROPERTIES_H_
#define YCSB_C_PROPERTIES_H_

#include <string>
#include <map>
#include <fstream>
#include <cassert>
#include "utils.h"

namespace utils {

class Properties {
 public:
  const std::string &GetProperty(const std::string &key,
      const std::string &default_value = std::string()) const;
  const std::string &operator[](const std::string &key) const;

  long int GetIntProperty(const std::string &key) const;
  //int operator[](const std::string &key) const;

  const std::map<std::string, std::string> &properties() const;

  void SetProperty(const std::string &key, const std::string &value);
  void SetPropertyForLoad();
  bool Load(std::ifstream &input);
 private:
  std::map<std::string, std::string> properties_;
};

inline const std::string &Properties::GetProperty(const std::string &key,
    const std::string &default_value) const {
  std::map<std::string, std::string>::const_iterator it = properties_.find(key);
  if (properties_.end() == it) {
    return default_value;
  } else return it->second;
}

inline const std::string &Properties::operator[](const std::string &key) const {
  return properties_.at(key);
}

inline long int Properties::GetIntProperty(const std::string &key) const {
  std::string s = GetProperty(key);
  return stol(s);
}

// inline int Properties::operator[](const std::string &key) const {
//   return GetProperty(key);
// }

inline const std::map<std::string, std::string> &Properties::properties() const {
  return properties_;
}

inline void Properties::SetProperty(const std::string &key,
                                    const std::string &value) {
  properties_[key] = value;
}

inline void Properties::SetPropertyForLoad() {
  //std::cout << "key=" << key << std::endl;
  //std::cout << "value=" << value << std::endl;
  std::string key, value;
  key = "recordcount";
  value = "10";
  properties_[key] = value;

  key = "workload";
  value = "com.yahoo.ycsb.workloads.CoreWorkload";
  properties_[key] = value;

  key = "fieldcount";
  value = "1";
  properties_[key] = value;
}

inline bool Properties::Load(std::ifstream &input) {
  if (!input.is_open()) throw utils::Exception("File not open!");

  while (!input.eof() && !input.bad()) {
    std::string line;
    std::getline(input, line);
    if (line[0] == '#') continue;
    size_t pos = line.find_first_of('=');
    if (pos == std::string::npos) continue;
    SetProperty(Trim(line.substr(0, pos)), Trim(line.substr(pos + 1)));
  }
  return true;
}

} // utils

#endif // YCSB_C_PROPERTIES_H_
