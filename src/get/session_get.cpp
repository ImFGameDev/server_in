#include <iostream>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <utility>

#include "../../include/sessions/get/session_get.h"

namespace main_player::server::http
{
	void session_get::cleanup()
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

	void session_get::process_request()
	{
		try
		{
			if(!_req)
			{
				cleanup();
				return;
			}

			std::cout << "Received " << _req->method_string() << " " << _req->target() << std::endl;

			std::string target = _req->target();
			std::string query_string;

			std::size_t query_pos = target.find('?');

			if(query_pos != std::string::npos) query_string = target.substr(query_pos + 1);
			else query_string = "";

			main_player::core::debug::debug_system::log("session_get", "GET request: " + target);

			std::string result = _on_request(query_string);

			auto response = create_response(boost::beast::http::status::ok, result);

			response.set(boost::beast::http::field::content_type, "application/json");

			main_player::core::debug::debug_system::log_green("session_get", "GET response: " + result);

			send_response(std::move(response));
		}
		catch(const std::exception& e)
		{
			main_player::core::debug::debug_system::error("session_get", "GET error: " + std::string(e.what()));

			send_response(create_response(boost::beast::http::status::bad_request,
			                              "Error processing GET request: " + std::string(e.what())));
		}
	}

	void session_get::read_request()
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

	boost::beast::http::response<boost::beast::http::string_body> session_get::create_response(boost::beast::http::status status, const std::string& body)
	{
		if(!_req) return boost::beast::http::response<boost::beast::http::string_body>{status, 11};

		boost::beast::http::response<boost::beast::http::string_body> response {status, _req->version()};

		response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
		response.set(boost::beast::http::field::content_type, "text/plain");
		response.body() = body;
		response.prepare_payload();
		response.keep_alive(_req->keep_alive());

		return response;
	}

	void session_get::send_response(boost::beast::http::response<boost::beast::http::string_body>&& response)
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
	session_get::session_get(boost::asio::ip::tcp::socket* socket, std::function<std::string(const std::string&)> on_request)
	{
		_socket = new boost::asio::ip::tcp::socket(std::move(*socket));
		_on_request = std::move(on_request);

		_buffer = nullptr;
		_req = nullptr;
		_res = nullptr;
		_is_reading = false;

		read_request();
	}

	session_get::~session_get()
	{
		cleanup();
	}
}
