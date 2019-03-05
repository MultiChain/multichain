// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_RPCASIO_H
#define BITCOIN_RPCASIO_H

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <rpc/rpcprotocol.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

/**
 * IOStream device that speaks SSL but can also speak non-SSL
 */
template <typename Protocol>
class SSLIOStreamDevice : public boost::iostreams::device<boost::iostreams::bidirectional> {
public:
    SSLIOStreamDevice(boost::asio::ssl::stream<typename Protocol::socket> &streamIn, bool fUseSSLIn) : stream(streamIn)
    {
        fUseSSL = fUseSSLIn;
        fNeedHandshake = fUseSSLIn;
    }

    void handshake(boost::asio::ssl::stream_base::handshake_type role)
    {
        if (!fNeedHandshake) return;
        fNeedHandshake = false;
        stream.handshake(role);
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::server); // HTTPS servers read first
        if (fUseSSL) return stream.read_some(boost::asio::buffer(s, n));
        return stream.next_layer().read_some(boost::asio::buffer(s, n));
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::client); // HTTPS clients write first
        if (fUseSSL) return boost::asio::write(stream, boost::asio::buffer(s, n));
        return boost::asio::write(stream.next_layer(), boost::asio::buffer(s, n));
    }
    bool connect(const std::string& server, const std::string& port)
    {
        using namespace boost::asio::ip;
        tcp::resolver resolver(stream.get_io_service());
        tcp::resolver::iterator endpoint_iterator;
#if BOOST_VERSION >= 104300
        try {
#endif
            // The default query (flags address_configured) tries IPv6 if
            // non-localhost IPv6 configured, and IPv4 if non-localhost IPv4
            // configured.
            tcp::resolver::query query(server.c_str(), port.c_str());
            endpoint_iterator = resolver.resolve(query);
#if BOOST_VERSION >= 104300
        } catch(boost::system::system_error &e)
        {
            // If we at first don't succeed, try blanket lookup (IPv4+IPv6 independent of configured interfaces)
            tcp::resolver::query query(server.c_str(), port.c_str(), resolver_query_base::flags());
            endpoint_iterator = resolver.resolve(query);
        }
#endif
        boost::system::error_code error = boost::asio::error::host_not_found;
        tcp::resolver::iterator end;
        while (error && endpoint_iterator != end)
        {
            stream.lowest_layer().close();
            stream.lowest_layer().connect(*endpoint_iterator++, error);
        }
        if (error)
            return false;
        return true;
    }

private:
    bool fNeedHandshake;
    bool fUseSSL;
    boost::asio::ssl::stream<typename Protocol::socket>& stream;
};


#endif // BITCOIN_RPCASIO_H
