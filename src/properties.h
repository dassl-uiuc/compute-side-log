#pragma once

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <string>
#include <iostream>
#include <string.h>

class Exception : public std::exception {
   public:
    Exception(const std::string &message) : message_(message) {}
    const char *what() const noexcept { return message_.c_str(); }

   private:
    std::string message_;
};

inline bool StrToBool(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if (str == "true" || str == "1") {
        return true;
    } else if (str == "false" || str == "0") {
        return false;
    } else {
        throw Exception("Invalid bool string: " + str);
    }
}

inline std::string Trim(const std::string &str) {
    auto front = std::find_if_not(str.begin(), str.end(), [](int c) { return std::isspace(c); });
    return std::string(front, std::find_if_not(str.rbegin(), std::string::const_reverse_iterator(front), [](int c) {
                                  return std::isspace(c);
                              }).base());
}

inline bool StrStartWith(const char *str, const char *pre) {
    return strncmp(str, pre, strlen(pre)) == 0;
}

class Properties {
   public:
    std::string GetProperty(const std::string &key, const std::string &default_value = std::string()) const;
    const std::string &operator[](const std::string &key) const;
    void SetProperty(const std::string &key, const std::string &value);
    bool ContainsKey(const std::string &key) const;
    void Load(std::ifstream &input);

   private:
    std::map<std::string, std::string> properties_;
};

inline std::string Properties::GetProperty(const std::string &key, const std::string &default_value) const {
    std::map<std::string, std::string>::const_iterator it = properties_.find(key);
    if (properties_.end() == it) {
        return default_value;
    } else {
        return it->second;
    }
}

void UsageMessage(const char *command);

void ParseCommandLine(int argc, const char *argv[], Properties &props);

void UsageMessage(const char *command);

inline const std::string &Properties::operator[](const std::string &key) const { return properties_.at(key); }

inline void Properties::SetProperty(const std::string &key, const std::string &value) { properties_[key] = value; }

inline bool Properties::ContainsKey(const std::string &key) const { return properties_.find(key) != properties_.end(); }

inline void Properties::Load(std::ifstream &input) {
    if (!input.is_open()) {
        throw Exception("File not open!");
    }

    while (!input.eof() && !input.bad()) {
        std::string line;
        std::getline(input, line);
        if (line[0] == '#') continue;
        size_t pos = line.find_first_of('=');
        if (pos == std::string::npos) continue;
        SetProperty(Trim(line.substr(0, pos)), Trim(line.substr(pos + 1)));
    }
}
