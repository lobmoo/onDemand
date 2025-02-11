#include "net_interface.h"

TcpServer::TcpServer(boost::asio::io_context& io_context, short port, ITcpServer& handler)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), handler_(handler) {
  start_accept();
}

void TcpServer::start_accept() {
  auto new_socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
  acceptor_.async_accept(*new_socket, [this, new_socket](boost::system::error_code ec) {
    if (!ec) {
      handler_.onClientConnected(new_socket);
      do_read(new_socket);
    }
    start_accept();
  });
}

void TcpServer::do_read(std::shared_ptr<tcp::socket> socket) {
  auto buffer = std::make_shared<std::vector<char>>(1024);
  socket->async_read_some(
      boost::asio::buffer(*buffer), [this, socket, buffer](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          handler_.onDataReceived(socket, std::string(buffer->data(), length));
          do_read(socket);
        }
      });
}

void TcpServer::write(std::shared_ptr<tcp::socket> socket, const std::string& message) {
  boost::asio::async_write(
      *socket, boost::asio::buffer(message), [socket](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "Server Write error: " << ec.message() << std::endl;
        }
      });
}

TcpClient::TcpClient(
    boost::asio::io_context& io_context, const std::string& host, const std::string& port, ITcpClient& handler)
    : socket_(io_context), resolver_(io_context), handler_(handler) {
  connect(host, port);
}

void TcpClient::connect(const std::string& host, const std::string& port) {
  resolver_.async_resolve(host, port, [this](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
    if (!ec) {
      boost::asio::async_connect(socket_, endpoints, [this](boost::system::error_code ec, tcp::endpoint) {
        if (!ec) {
          handler_.onConnected();
          do_read();
        }
      });
    }
  });
}

void TcpClient::do_read() {
  socket_.async_read_some(
      boost::asio::buffer(data_, max_length), [this](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          handler_.onDataReceived(std::string(data_, length));
          do_read();
        }
      });
}

void TcpClient::write(const std::string& message) {
  boost::asio::async_write(
      socket_, boost::asio::buffer(message), [this](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "Client Write error: " << ec.message() << std::endl;
        }
      });
}
