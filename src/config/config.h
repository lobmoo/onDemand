#ifndef CONFIG_H
#define CONFIG_H

#include <fstream>
#include <iostream>
#include <json.hpp>
#include <vector>

using json = nlohmann::json;

class Config {
 private:
  std::string m_configPath;
  json m_config;
  bool m_isLoaded;

 public:
  Config(const std::string& configPath);

  void load();

  bool isLoaded() const;


  int getInt(const std::string& key, int defaultValue = 0) const;
  int getNestedInt(const std::vector<std::string>& keys, int defaultValue = 0) const;

  float getFloat(const std::string& key, float defaultValue = 0.0f) const;
  float getNestedFloat(const std::vector<std::string>& keys, float defaultValue = 0.0f) const;

  std::string getString(const std::string& key, const std::string& defaultValue = "") const;
  std::string getNestedString(const std::vector<std::string>& keys, const std::string& defaultValue = "") const;

  bool getBool(const std::string& key, bool defaultValue = false) const;
  bool getNestedBool(const std::vector<std::string>& keys, bool defaultValue = false) const;

  std::vector<int> getIntArray(const std::string& key, const std::vector<int>& defaultValue = {}) const;
  std::vector<int> getNestedIntArray(
      const std::vector<std::string>& keys, const std::vector<int>& defaultValue = {}) const;

  std::vector<float> getFloatArray(const std::string& key, const std::vector<float>& defaultValue = {}) const;
  std::vector<float> getNestedFloatArray(
      const std::vector<std::string>& keys, const std::vector<float>& defaultValue = {}) const;

  std::vector<std::string> getStringArray(
      const std::string& key, const std::vector<std::string>& defaultValue = {}) const;
  std::vector<std::string> getNestedStringArray(
      const std::vector<std::string>& keys, const std::vector<std::string>& defaultValue = {}) const;

  std::vector<bool> getBoolArray(const std::string& key, const std::vector<bool>& defaultValue = {}) const;
  std::vector<bool> getNestedBoolArray(
      const std::vector<std::string>& keys, const std::vector<bool>& defaultValue = {}) const;

  void setInt(const std::string& key, int value);
  void setNestedInt(const std::vector<std::string>& keys, int value);

  void setFloat(const std::string& key, float value);
  void setNestedFloat(const std::vector<std::string>& keys, float value);

  void setString(const std::string& key, const std::string& value);
  void setNestedString(const std::vector<std::string>& keys, const std::string& value);

  void setBool(const std::string& key, bool value);
  void setNestedBool(const std::vector<std::string>& keys, bool value);

  void setIntArray(const std::string& key, const std::vector<int>& value);
  void setNestedIntArray(const std::vector<std::string>& keys, const std::vector<int>& value);

  void setFloatArray(const std::string& key, const std::vector<float>& value);
  void setNestedFloatArray(const std::vector<std::string>& keys, const std::vector<float>& value);

  void setStringArray(const std::string& key, const std::vector<std::string>& value);
  void setNestedStringArray(const std::vector<std::string>& keys, const std::vector<std::string>& value);

  void setBoolArray(const std::string& key, const std::vector<bool>& value);
  void setNestedBoolArray(const std::vector<std::string>& keys, const std::vector<bool>& value);

  void save(const std::string& path = "");
};

#endif  // CONFIG_H