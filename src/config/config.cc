#include "config.h"

Config::Config(const std::string& configPath) : m_configPath(configPath), m_isLoaded(false) {}

void Config::load() {
  try {
    std::ifstream configFile(m_configPath);
    if (!configFile.is_open()) {
      throw std::runtime_error("Failed to open configuration file.");
    }
    configFile >> m_config;
    m_isLoaded = true;
  } catch (const json::parse_error& e) {
    throw std::runtime_error("JSON parse error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    throw std::runtime_error("Error loading configuration file: " + std::string(e.what()));
  }
}

bool Config::isLoaded() const { return m_isLoaded; }

int Config::getInt(const std::string& key, int defaultValue) const {
  try {
    return m_config.at(key).get<int>();
  } catch (...) {
    return defaultValue;
  }
}

int Config::getNestedInt(const std::vector<std::string>& keys, int defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<int>();
  } catch (...) {
    return defaultValue;
  }
}

float Config::getFloat(const std::string& key, float defaultValue) const {
  try {
    return m_config.at(key).get<float>();
  } catch (...) {
    return defaultValue;
  }
}

float Config::getNestedFloat(const std::vector<std::string>& keys, float defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<float>();
  } catch (...) {
    return defaultValue;
  }
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
  try {
    return m_config.at(key).get<std::string>();
  } catch (...) {
    return defaultValue;
  }
}

std::string Config::getNestedString(const std::vector<std::string>& keys, const std::string& defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<std::string>();
  } catch (...) {
    return defaultValue;
  }
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
  try {
    return m_config.at(key).get<bool>();
  } catch (...) {
    return defaultValue;
  }
}

bool Config::getNestedBool(const std::vector<std::string>& keys, bool defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<bool>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<int> Config::getIntArray(const std::string& key, const std::vector<int>& defaultValue) const {
  try {
    return m_config.at(key).get<std::vector<int>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<int> Config::getNestedIntArray(
    const std::vector<std::string>& keys, const std::vector<int>& defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<std::vector<int>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<float> Config::getFloatArray(const std::string& key, const std::vector<float>& defaultValue) const {
  try {
    return m_config.at(key).get<std::vector<float>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<float> Config::getNestedFloatArray(
    const std::vector<std::string>& keys, const std::vector<float>& defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<std::vector<float>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<std::string> Config::getStringArray(
    const std::string& key, const std::vector<std::string>& defaultValue) const {
  try {
    return m_config.at(key).get<std::vector<std::string>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<std::string> Config::getNestedStringArray(
    const std::vector<std::string>& keys, const std::vector<std::string>& defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<std::vector<std::string>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<bool> Config::getBoolArray(const std::string& key, const std::vector<bool>& defaultValue) const {
  try {
    return m_config.at(key).get<std::vector<bool>>();
  } catch (...) {
    return defaultValue;
  }
}

std::vector<bool> Config::getNestedBoolArray(
    const std::vector<std::string>& keys, const std::vector<bool>& defaultValue) const {
  try {
    json nestedValue = m_config;
    for (const auto& key : keys) {
      nestedValue = nestedValue.at(key);
    }
    return nestedValue.get<std::vector<bool>>();
  } catch (...) {
    return defaultValue;
  }
}

void Config::setInt(const std::string& key, int value) { m_config[key] = value; }

void Config::setNestedInt(const std::vector<std::string>& keys, int value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setFloat(const std::string& key, float value) { m_config[key] = value; }

void Config::setNestedFloat(const std::vector<std::string>& keys, float value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setString(const std::string& key, const std::string& value) { m_config[key] = value; }

void Config::setNestedString(const std::vector<std::string>& keys, const std::string& value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setBool(const std::string& key, bool value) { m_config[key] = value; }

void Config::setNestedBool(const std::vector<std::string>& keys, bool value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setIntArray(const std::string& key, const std::vector<int>& value) { m_config[key] = value; }

void Config::setNestedIntArray(const std::vector<std::string>& keys, const std::vector<int>& value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setFloatArray(const std::string& key, const std::vector<float>& value) { m_config[key] = value; }

void Config::setNestedFloatArray(const std::vector<std::string>& keys, const std::vector<float>& value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setStringArray(const std::string& key, const std::vector<std::string>& value) { m_config[key] = value; }

void Config::setNestedStringArray(const std::vector<std::string>& keys, const std::vector<std::string>& value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::setBoolArray(const std::string& key, const std::vector<bool>& value) { m_config[key] = value; }

void Config::setNestedBoolArray(const std::vector<std::string>& keys, const std::vector<bool>& value) {
  json* nestedValue = &m_config;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i == keys.size() - 1) {
      (*nestedValue)[keys[i]] = value;
    } else {
      if ((*nestedValue).find(keys[i]) == (*nestedValue).end()) {
        (*nestedValue)[keys[i]] = json();
      }
      nestedValue = &((*nestedValue)[keys[i]]);
    }
  }
}

void Config::save(const std::string& path) {
  std::string savePath = path.empty() ? m_configPath : path;

  try {
    std::ofstream configFile(savePath);
    if (!configFile.is_open()) {
      throw std::runtime_error("Failed to open configuration file for writing at path: " + savePath);
    }
    configFile << m_config.dump(4);
    std::cout << "Configuration saved successfully to: " << savePath << std::endl;
  } catch (const std::exception& e) {
    throw std::runtime_error("Error saving configuration file: " + std::string(e.what()));
  }
}