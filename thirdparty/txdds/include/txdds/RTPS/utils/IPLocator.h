#ifndef TXDDS_RTPS_IPLOCATOR_H
#define TXDDS_RTPS_IPLOCATOR_H

#include "txdds/RTPS/common/RTPSMessageTypes.h"
#include "txdds/txddsexport.h"

#include <cstdint>
#include <set>
#include <string>

namespace BaoSky::rtps
{
    class Locator;
    bool getipv4(std::vector<std::string> &interfaces);
    class TXDDS_API IPLocator
    {
    public:
        static void createLocator(int32_t kindin, const std::string &address, uint32_t portin, Locator &locator);

        static bool setIPv4(Locator &locator, const unsigned char *addr);

        static bool setIPv4(Locator &locator,
                            octet o1, octet o2, octet o3, octet o4);

        static bool setIPv4(Locator &locator, const std::string &ipv4);

        static bool setIPv4(Locator &destlocator, const Locator &origlocator);
        static const octet *getIPv4(
            const Locator &locator);

        static bool hasIPv4(const Locator &locator);

        static std::string toIPv4string(const Locator &locator);

        static bool copyIPv4(const Locator &locator, unsigned char *dest);

        static bool setIPv6(Locator &locator, const unsigned char *addr);

        static bool setIPv6(Locator &locator,
                            uint16_t group0,
                            uint16_t group1,
                            uint16_t group2,
                            uint16_t group3,
                            uint16_t group4,
                            uint16_t group5,
                            uint16_t group6,
                            uint16_t group7);

        static bool setIPv6(Locator &locator, const std::string &ipv6);

        static bool setIPv6(Locator &destlocator, const Locator &origlocator);

        static const octet *getIPv6(const Locator &locator);

        static bool hasIPv6(const Locator &locator);

        static std::string toIPv6string(const Locator &locator);

        static bool copyIPv6(const Locator &locator, unsigned char *dest);

        static bool ip(Locator &locator, const std::string &ip);

        static std::string ip_to_string(const Locator &locator);

        static bool setLogicalPort(Locator &locator, uint16_t port);

        static uint16_t getLogicalPort(const Locator &locator);

        static bool setPhysicalPort(Locator &locator, uint16_t port);

        static uint16_t getPhysicalPort(const Locator &locator);

        static bool setPortRTPS(Locator &locator, uint16_t port);

        static uint16_t getPortRTPS(Locator &locator);

        static bool isLocal(const Locator &locator);

        static bool isAny(const Locator &locator);

        static std::string to_string(const Locator &locator);

        static bool isMulticast(const Locator &locator);

        static bool isIPv4(const std::string &address);
        static bool isIPv6(const std::string &address);

    protected:
        static bool isEmpty(const Locator &locator);

        static bool isEmpty(const Locator &locator, uint16_t index);

        static bool IPv6isCorrect(const std::string &ipv6);

        static void ParseIPV6(uint16_t &initial_zeros, uint16_t &final_zeros, uint16_t &position_zeros, uint16_t &number_zeros, const std::string &ipv6);

        static bool fillIPV6LocatorWithInitialZero(Locator &locator, uint16_t initial_zeros, std::stringstream &ss);

        static bool fillIPV6LocatorWithFinalZero(Locator &locator, uint16_t final_zeros, std::stringstream &ss);

        static bool fillIPV6LocatorWithMidZero(Locator &locator, uint16_t position_zeros, uint16_t number_zeros, std::stringstream &ss);

        static bool fillIPV6LocatorWithNoZero(Locator &locator, std::stringstream &ss);

    private:
        IPLocator() = delete;
        ~IPLocator() = delete;
    };
}

#endif