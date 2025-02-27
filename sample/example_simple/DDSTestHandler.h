#pragma once

#include <functional>
#include <unordered_map>

#include "fastdds_wrapper/DDSParticipantManager.h"

class DDSTestHandler : public DDSParticipantManager {
 public:
  DDSTestHandler(int domainId);
  ~DDSTestHandler();

 protected:
  ParticipantQosHandler createParticipantQos(
      const std::string &participant_name, uint16_t listen_port,
      const std::vector<std::string> &peer_locators) override;
};