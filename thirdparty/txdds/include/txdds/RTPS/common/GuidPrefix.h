#ifndef TXDDS_RTPS_GuidPrefix_H
#define TXDDS_RTPS_GuidPrefix_H

#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace BaoSky::rtps
{
    // GuidPrefix
    struct GuidPrefix
    {
        static constexpr unsigned int mLength = 12;
        unsigned char mValue[mLength];
        GuidPrefix()
        {
            memset(mValue, 0, mLength);
        }

        bool operator==(const GuidPrefix &prefix) const
        {
            return memcmp(mValue, prefix.mValue, mLength) == 0;
        }
        bool operator!=(const GuidPrefix &prefix) const
        {
            return memcmp(mValue, prefix.mValue, mLength) != 0;
        }
        bool operator<(const GuidPrefix &prefix) const
        {
            return memcmp(mValue, prefix.mValue, mLength) < 0;
        }
        static int cmp(
            const GuidPrefix &prefix1,
            const GuidPrefix &prefix2)
        {
            return std::memcmp(prefix1.mValue, prefix2.mValue, mLength);
        }

        static GuidPrefix Unknown()
        {
            return GuidPrefix();
        }
        bool IsOnSameHost(const GuidPrefix &prefix) const
        {
            return memcmp(mValue, prefix.mValue, 4) == 0;
        }
        bool IsOnSameProcess(const GuidPrefix &prefix) const
        {
            return memcmp(mValue, prefix.mValue, 8) == 0;
        }
        // todo:is_from_this_host,is_from_this_process
    };
    const GuidPrefix GUIDPREFIX_UNKNOWN;

    inline std::ostream &operator<<(
        std::ostream &output,
        const GuidPrefix &guiP)
    {
        std::stringstream ss;
        ss << std::hex;
        char old_fill = ss.fill('0');
        for (uint8_t i = 0; i < 11; ++i)
        {
            ss << std::setw(2) << (int)guiP.mValue[i] << ".";
        }
        ss << std::setw(2) << (int)guiP.mValue[11];
        ss.fill(old_fill);
        ss << std::dec;
        return output << ss.str();
    }
}

#endif