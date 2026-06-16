/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/transport/TransportConfig.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_TRANSPORTCONFIG
#define TXDDS_RTPS_TRANSPORTCONFIG

#include <memory>
#include <list>
#include <asio/asio.hpp>
#include <asio/ssl.hpp>
#include <txdds/RTPS/common/Locator.h>

const size_t SHM_SLOTNUM = 512;
const size_t SHM_SLOTSIZE = 64 * 1024;

namespace BaoSky::rtps
{
    enum eTransportKind
    {
        UnknownKind,
        UDPTransportKind,
        TCPTransportKind,
        SHMTransportKind
    };

    struct BasicTransportConfig
    {
        std::string mConfigName = "";
        std::string mThreadConfigName = "";
        eTransportKind mKind = UnknownKind;
        uint32_t mSendBufferSize = 0;
        uint32_t mRecvBufferSize = 0;
        std::string mInterfaceName = "";
    };

    struct UDPTransportConfig : public BasicTransportConfig
    {
        UDPTransportConfig()
        {
            mKind = UDPTransportKind;
        }
        std::vector<std::string> mLocalIP{};
        uint32_t mLocalPort = 0;
    };

    struct SHMTransportConfig : public BasicTransportConfig
    {
        SHMTransportConfig()
        {
            mKind = SHMTransportKind;
            mShmName = "";
        }
        std::string mShmName;
        uint16_t mSlotNum = SHM_SLOTNUM;
        uint32_t mSlotSize = SHM_SLOTSIZE;
    };
    struct TLSConfig
    {
        enum TLSOptions : uint32_t
        {
            NONE = 0,                     // 0000 0000 0000
            DEFAULT_WORKAROUNDS = 1 << 0, // 0000 0000 0001
            NO_COMPRESSION = 1 << 1,      // 0000 0000 0010
            NO_SSLV2 = 1 << 2,            // 0000 0000 0100
            NO_SSLV3 = 1 << 3,            // 0000 0000 1000
            NO_TLSV1 = 1 << 4,            // 0000 0001 0000
            NO_TLSV1_1 = 1 << 5,          // 0000 0010 0000
            NO_TLSV1_2 = 1 << 6,          // 0000 0100 0000
            NO_TLSV1_3 = 1 << 7,          // 0000 1000 0000
            SINGLE_DH_USE = 1 << 8        // 0001 0000 0000
        };

        enum TLSVerifyMode : uint8_t
        {
            UNUSED = 0,                           // 0000 0000
            VERIFY_NONE = 1 << 0,                 // 0000 0001
            VERIFY_PEER = 1 << 1,                 // 0000 0010
            VERIFY_FAIL_IF_NO_PEER_CERT = 1 << 2, // 0000 0100
            VERIFY_CLIENT_ONCE = 1 << 3           // 0000 1000
        };

        uint32_t options;
        std::string password;
        asio::ssl::verify_mode tls_verify_mode;
        std::string server_name;
        std::string cert_chain_file;
        std::string private_key_file;
        std::string tmp_dh_file;
        std::string verify_file;
        std::vector<std::string> verify_paths;
        bool default_verify_path;
        int32_t verify_depth;
        std::string rsa_private_key_file;
        asio::ssl::stream_base::handshake_type handshake_role;
        TLSConfig()
        {
            options = 0;
            password = "";
            tls_verify_mode = 0x00;
            server_name = "";
            cert_chain_file = "";
            private_key_file = "";
            tmp_dh_file = "";
            verify_file = "";
            verify_paths.clear();
            default_verify_path = false;
            verify_depth = -1;
            rsa_private_key_file = "";
        }

        void SetVerifyMode(TLSVerifyMode mode)
        {
            if (mode == TLSVerifyMode::VERIFY_NONE)
            {
                tls_verify_mode |= asio::ssl::verify_none;
            }
            else if (mode == TLSVerifyMode::VERIFY_PEER)
            {
                tls_verify_mode |= asio::ssl::verify_peer;
            }
            else if (mode == TLSVerifyMode::VERIFY_FAIL_IF_NO_PEER_CERT)
            {
                tls_verify_mode |= asio::ssl::verify_fail_if_no_peer_cert;
            }
            else if (mode == TLSVerifyMode::VERIFY_CLIENT_ONCE)
            {
                tls_verify_mode |= asio::ssl::verify_client_once;
            }
        }

        void SetOption(TLSOptions option)
        {
            if (option == TLSOptions::DEFAULT_WORKAROUNDS)
            {
                options |= asio::ssl::context::default_workarounds;
            }

            if (option == TLSOptions::NO_COMPRESSION)
            {
                options |= asio::ssl::context::no_compression;
            }

            if (option == TLSOptions::NO_SSLV2)
            {
                options |= asio::ssl::context::no_sslv2;
            }

            if (option == TLSOptions::NO_SSLV3)
            {
                options |= asio::ssl::context::no_sslv3;
            }

            if (option == TLSOptions::NO_TLSV1)
            {
                options |= asio::ssl::context::no_tlsv1;
            }

            if (option == TLSOptions::NO_TLSV1_1)
            {
                options |= asio::ssl::context::no_tlsv1_1;
            }

            if (option == TLSOptions::NO_TLSV1_2)
            {
                options |= asio::ssl::context::no_tlsv1_2;
            }

            if (option == TLSOptions::SINGLE_DH_USE)
            {
                options |= asio::ssl::context::single_dh_use;
            }
        }
    };

    struct TCPTransportConfig : public BasicTransportConfig
    {
        TCPTransportConfig()
        {
            mKind = TCPTransportKind;
        }
        bool mIsServer = false;
        std::string mLocalIP = "";
        uint32_t mListenPort = 0;
        std::string mPeerIP = "";
        uint32_t mPeerPort = 0;
        bool mIsUseTLS = false;
        bool CRCCheck = false;
        bool no_delay = false;
        bool noBlockingsend = true;
        TLSConfig tlsConfig;
        uint16_t logic_port = 0;
        uint16_t physical_port = 0;
        uint16_t channel_port = 0;
    };
}

#endif