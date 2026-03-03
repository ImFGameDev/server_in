#include <iostream>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <utility>

#include "../../include/sessions/post/session_post.h"

namespace main_player::server::http
{
	void session_post::cleanup()
	{
		if(_req)
		{
			delete _req;
			_req = nullptr;
		}

		if(_res)
		{
			delete _res;
			_res = nullptr;
		}

		if(_buffer)
		{
			delete _buffer;
			_buffer = nullptr;
		}

		if(_socket)
		{
			boost::beast::error_code ec;

			_socket->close(ec);

			delete _socket;
			_socket = nullptr;
		}

		_is_reading = false;
	}

	void session_post::process_request()
	{
		if(!_req)
		{
			cleanup();
			return;
		}

		std::cout << "Received " << _req->method_string() << " " << _req->target() << std::endl;

		try
		{
			const std::string& body = _req->body();

			main_player::core::debug::debug_system::log_green("session_post", "POST data: " + body);

			std::string result = _on_request(body);

			auto response = create_response(boost::beast::http::status::ok, result);

			main_player::core::debug::debug_system::log_green("session_post", "POST response: " + result);

			send_response(std::move(response));
		}
		catch(const std::exception& e)
		{
			main_player::core::debug::debug_system::error("session_post", "POST error: " + std::string(e.what()));
			send_response(create_response(boost::beast::http::status::bad_request, "Error processing request"));
		}
	}

	void session_post::read_request()
	{
		_req = new boost::beast::http::request<boost::beast::http::string_body>();
		_buffer = new boost::beast::flat_buffer(8192);
		_is_reading = true;

		async_read(*_socket, *_buffer, *_req, [this](boost::beast::error_code ec, std::size_t bytes_transferred)
		{
			if(!ec && bytes_transferred > 0) process_request();
			else cleanup();
		});
	}

	boost::beast::http::response<boost::beast::http::string_body> session_post::create_response(boost::beast::http::status status, const std::string& body)
	{
		if(!_req) return boost::beast::http::response<boost::beast::http::string_body>{status, 11};

		boost::beast::http::response<boost::beast::http::string_body> response{status, _req->version()};

		response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
		response.set(boost::beast::http::field::content_type, "application/json");
		response.body() = body;
		response.prepare_payload();
		response.keep_alive(_req->keep_alive());

		return response;
	}

	void session_post::send_response(boost::beast::http::response<boost::beast::http::string_body>&& response)
	{
		_res = new boost::beast::http::response<boost::beast::http::string_body>(std::move(response));

		boost::beast::http::async_write(*_socket, *_res, [this](boost::beast::error_code ec, std::size_t)
		{
			if(!ec)
			{
				boost::beast::error_code shutdown_ec;
				_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_send, shutdown_ec);
			}
			cleanup();
		});
	}

	//Public:
	session_post::session_post(boost::asio::ip::tcp::socket* socket, std::function<std::string(const std::string&)> on_request)
	{
		_socket = new boost::asio::ip::tcp::socket(std::move(*socket));
		_on_request = std::move(on_request);
		_buffer = nullptr;
		_req = nullptr;
		_res = nullptr;
		_is_reading = false;

		read_request();
	}

	session_post::~session_post()
	{
		cleanup();
	}
}
