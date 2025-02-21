#ifndef CONFIG_H
#define CONFIG_H

#include <yaml-cpp/yaml.h>

#include <mutex>
#include <string>

/*≈‰÷√Ω·ππ*/

struct ConfigData {
  std::string server_ip = "127.0.0.1";  
  int timeout = 30;                     
};


class Config {
 public:
 ~Config();  

  static Config& getInstance();

  void loadConfig(const std::string& filename);

  const ConfigData* getConfig();

 private:
  Config();                                   
  Config(const Config&) = delete;             
  Config& operator=(const Config&) = delete;  

  std::unique_ptr<ConfigData> data = nullptr;    
  std::mutex mutex_;  
};

#endif  // CONFIG_H