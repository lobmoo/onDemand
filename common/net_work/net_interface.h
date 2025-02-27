#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <memory>

using boost::asio::ip::tcp;

class ITcpServer {
public:
    virtual ~ITcpServer() = default;
    virtual void onClientConnected(std::shared_ptr<tcp::socket> socket) = 0;
    virtual void onDataReceived(std::shared_ptr<tcp::socket> socket, const std::string& data) = 0;
};

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context, short port, ITcpServer& handler);
    void write(std::shared_ptr<tcp::socket> socket, const std::string& message);

private:
    void start_accept();
    void do_read(std::shared_ptr<tcp::socket> socket);

    tcp::acceptor acceptor_;
    ITcpServer& handler_;
};


class ITcpClient {
public:
    virtual ~ITcpClient() = default;
    virtual void onConnected() = 0;
    virtual void onDataReceived(const std::string& data) = 0;
};

class TcpClient {
public:
    TcpClient(boost::asio::io_context& io_context, const std::string& host, const std::string& port, ITcpClient& handler);
    void write(const std::string& message);

private:
    void connect(const std::string& host, const std::string& port);
    void do_read();

    tcp::socket socket_;
    tcp::resolver resolver_;
    ITcpClient& handler_;
    enum { max_length = 1024 };
    char data_[max_length] = {};
};


