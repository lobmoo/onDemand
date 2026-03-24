#ifndef TXDDS_RTPS_FIXEDSTRING_H
#define TXDDS_RTPS_FIXEDSTRING_H

#include <string>
#include <cstring>

namespace BaoSky::rtps
{
    template <size_t MAX_CHARS>
    struct FixedString
    {
    public:
        static constexpr size_t max_size = MAX_CHARS;

        FixedString() noexcept
        {
            memset(string_data, 0, sizeof(string_data));
            string_len = 0;
        }

        FixedString(const char *c_array, size_t n_chars) noexcept
        {
            assign(c_array, n_chars);
        }

        FixedString &assign(const char *c_array, size_t n_chars) noexcept
        {
            string_len = (nullptr == c_array) ? 0 : (MAX_CHARS < n_chars) ? MAX_CHARS
                                                                          : n_chars;
            if (0 < string_len)
            {
                memcpy(string_data, c_array, string_len);
            }
            return *this;
        }

        FixedString(const char *c_string) noexcept : FixedString()
        {
            set(c_string != nullptr ? c_string : "");
        }

        FixedString &operator=(const char *c_string) noexcept
        {
            set(c_string != nullptr ? c_string : "");
            return *this;
        }

        FixedString(const std::string &str) noexcept : FixedString()
        {
            set(str.c_str());
        }

        FixedString &operator=(const std::string &str) noexcept
        {
            set(str.c_str());
            return *this;
        }

        template <size_t N>
        FixedString &operator=(const FixedString<N> &rhs) noexcept
        {
            set(rhs.c_str());
            return *this;
        }

        const char *c_str() const noexcept
        {
            return string_data;
        }

        std::string to_string() const
        {
            return std::string(string_data);
        }

        bool operator==(const char *rhs) const noexcept
        {
            return strncmp(string_data, rhs, MAX_CHARS) == 0;
        }

        bool operator==(const std::string &rhs) const noexcept
        {
            return strncmp(string_data, rhs.c_str(), MAX_CHARS) == 0;
        }

        template <size_t N>
        bool operator==(const FixedString<N> &rhs) const noexcept
        {
            return strncmp(string_data, rhs.c_str(), MAX_CHARS) == 0;
        }

        bool operator!=(const char *rhs) const noexcept
        {
            return strncmp(string_data, rhs, MAX_CHARS) != 0;
        }

        bool operator!=(const std::string &rhs) const noexcept
        {
            return strncmp(string_data, rhs.c_str(), MAX_CHARS) != 0;
        }

        template <size_t N>
        bool operator!=(const FixedString<N> &rhs) const noexcept
        {
            return strncmp(string_data, rhs.c_str(), MAX_CHARS) != 0;
        }

        template <size_t N>
        bool operator<(const FixedString<N> &rhs) const noexcept
        {
            return 0 > compare(rhs);
        }

        template <size_t N>
        bool operator>(const FixedString<N> &rhs) const noexcept
        {
            return 0 < compare(rhs);
        }

        bool operator<(const std::string &rhs) const noexcept
        {
            return 0 > compare(rhs);
        }

        bool operator>(const std::string &rhs) const noexcept
        {
            return 0 < compare(rhs);
        }

        operator const char *() const noexcept
        {
            return c_str();
        }

        size_t size() const noexcept
        {
            return string_len;
        }

        int compare(const char *str) const noexcept
        {
            return strncmp(string_data, str, MAX_CHARS);
        }

        int compare(const std::string &str) const noexcept
        {
            return strncmp(string_data, str.c_str(), MAX_CHARS);
        }

        template <size_t N>
        int compare(const FixedString<N> &str) const noexcept
        {
            return strncmp(string_data, str.c_str(), MAX_CHARS);
        }

    private:
        void set(const char *c_string) noexcept
        {
            char *result = (char *)memcpy(string_data, c_string, '\0', MAX_CHARS);
            string_len = (result == nullptr) ? MAX_CHARS : (size_t)(result - string_data) - 1u;
        }

        char string_data[MAX_CHARS + 1];
        size_t string_len;
    };

    using String256 = FixedString<255>;

}

#endif
