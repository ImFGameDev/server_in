#ifndef POST_ACCEPTOR_H
#define POST_ACCEPTOR_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>

namespace main_player::server::http
{
	class port_acceptor
	{
	private:
		boost::asio::io_context* _io_context;
		boost::asio::ip::tcp::acceptor* _acceptor;
		std::function<void(boost::asio::ip::tcp::socket* socket)> _on_connect;
		std::uint16_t _port;

		void do_accept();

	public:
		port_acceptor(boost::asio::io_context* io_context, std::uint16_t port, const std::function<void(boost::asio::ip::tcp::socket*)>& on_connect);

		~port_acceptor();

		void start();
	};
}

#endif