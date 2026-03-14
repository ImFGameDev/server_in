#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <returnable/hash_events_getter.h>
#include <debug_system.h>

#include "../../include/sessions/tcp/in_session_tcp.h"
#include "commands.h"

namespace main_player::logic::connection
{
	const float _ping_pong = 30;

	//Public:
	in_session_tcp::in_session_tcp(boost::asio::ip::tcp::socket* socket)
		: _is_closing(false)
	{
		_socket = new boost::asio::ip::tcp::socket(std::move(*socket));
		_data_size = new char[4];
		_event = new main_player::core::actions::hash_events_getter<uint8_t, const std::string&>();

		_time_wait_ping = 0;
		_is_run = true;

		read_length();

		_event->add_listener(SESSION_PING, [this](const std::string&)
		{
			_time_wait_ping = 0;

			send(SESSION_PING, "");
		});
	}

	in_session_tcp::~in_session_tcp()
	{
		if (_socket)
		{
			try
			{
				boost::system::error_code ec;
				if (_socket->is_open())
				{
					_socket->close(ec);

					if (ec) std::cerr << "Destruct close error: " << ec.message() << std::endl;
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "Exception during close: " << e.what() << std::endl;
			}

			delete _socket;
		}

		delete _event;
		delete[] _data;
		delete[] _data_size;
	}

	//Private:
	void in_session_tcp::read_length()
	{
		if (!_socket->is_open())
		{
			close();
			return;
		}

		async_read(*_socket, boost::asio::buffer(_data_size, 4), [this](boost::system::error_code ec, size_t length) -> void
		{
			if (ec)
			{
				if (ec != boost::asio::error::operation_aborted)
					main_player::core::debug::debug_system::error("tcp_session",
					                                              "Socket read length failed: " + ec.message());

				close();
				return;
			}

			if (length != 4)
			{
				main_player::core::debug::debug_system::error("tcp_session",
				                                              "Invalid length header size: " + std::to_string(length));
				close();
				return;
			}

			int packet_length = 0;
			memcpy(&packet_length, _data_size, 4);

			if (packet_length < 1)
			{
				main_player::core::debug::debug_system::error("tcp_session",
				                                              "Invalid packet length: " + std::to_string(
					                                              packet_length));
				close();
				return;
			}

			read_data(packet_length);
		});
	}

	void in_session_tcp::read_data(int length)
	{
		delete[] _data;

		_data = new char[length];

		async_read(*_socket, boost::asio::buffer(_data, length),
		           [this, length](boost::system::error_code ec, size_t bytes_read) -> void
		           {
			           if (ec)
			           {
				           main_player::core::debug::debug_system::error(
					           "tcp_session", "Read data failed: " + ec.message());

				           close();
				           return;
			           }

			           if (bytes_read != static_cast<size_t>(length))
			           {
				           std::string log = "Expected: " + std::to_string(length) + ", Got: " + std::to_string(
					                             bytes_read);

				           main_player::core::debug::debug_system::error("tcp_session", "Incomplete data read. " + log);

				           close();
				           return;
			           }

			           std::uint8_t tag = _data[0];
			           int data_length = length - 1;

			           std::string json_data(_data + 1, data_length);
			           std::string log = "read: " + std::to_string(tag) + '/' + json_data;

			           main_player::core::debug::debug_system::log("tcp_session", log);

			           _event->invoke(tag, json_data);

			           read_length();
		           });
	}

	void in_session_tcp::send_internal(const uint8_t& tag, const std::string& json,
	                                   const std::function<void(boost::system::error_code, size_t)>& callback)
	{
		std::lock_guard<std::mutex> lock(_socket_mutex);
		if (!_socket->is_open())
		{
			if (callback) callback(boost::asio::error::not_connected, 0);

			close();
			return;
		}

		uint32_t data_length = json.length();
		uint32_t total_length = data_length + 1;

		char length_buffer[4];
		memcpy(length_buffer, &total_length, 4);

		uint8_t tag_buffer[1]{tag};

		std::vector<boost::asio::const_buffer> buffers;

		buffers.push_back(boost::asio::buffer(length_buffer, 4));
		buffers.push_back(boost::asio::buffer(tag_buffer, 1));
		buffers.push_back(boost::asio::buffer(json.data(), data_length));

		std::string log = "send: " + std::to_string(tag) + '/' + json;

		main_player::core::debug::debug_system::log("tcp_session", log);

		async_write(*_socket, buffers, callback);
	}

	void in_session_tcp::close()
	{
		if (_is_closing.exchange(true)) return;

		boost::system::error_code ec;
		std::lock_guard<std::mutex> lock(_socket_mutex);

		if (_socket && _socket->is_open())
		{
			_socket->close(ec);

			if (ec) main_player::core::debug::debug_system::error("tcp_session", "Close error: " + ec.message());
		}

		if (_close_callback)
		{
			main_player::core::debug::debug_system::log_green("tcp_session", "close callback");

			_close_callback();
		}
		else main_player::core::debug::debug_system::error("tcp_session", "close callback null");
	}

	//Public:
	void in_session_tcp::set_listener_close(const std::function<void()>& callback)
	{
		_close_callback = callback;
	}

	void in_session_tcp::remove_listener(const uint8_t& tag)
	{
		_event->remove_listeners(tag);
	}

	void in_session_tcp::add_listener(const uint8_t& tag, std::function<void(const std::string&)> func)
	{
		_event->add_listener(tag, func);
	}

	void in_session_tcp::send(const uint8_t& tag, const std::string& json)
	{
		auto callback = [this](boost::system::error_code ec, size_t) -> void
		{
			if (ec)
			{
				main_player::core::debug::debug_system::error("tcp_session",
				                                              "Error writing to socket: " + ec.message());
				close();
			}
		};

		send_internal(tag, json, callback);
	}

	void in_session_tcp::send(const uint8_t& tag, const std::string& json, std::function<void(bool)> on_send)
	{
		auto cachedCallback = std::move(on_send);
		auto callbackWrite = [this, cachedCallback](boost::system::error_code ec, size_t) -> void
		{
			if (!ec)
			{
				if (cachedCallback) cachedCallback(true);
				return;
			}

			main_player::core::debug::debug_system::error("tcp_session", "Error writing to socket: " + ec.message());
			if (cachedCallback) cachedCallback(false);
			close();
		};

		send_internal(tag, json, callbackWrite);
	}

	bool in_session_tcp::is_closed()
	{
		return !_is_run;
	}

	void in_session_tcp::tick(const float& delta)
	{
		if (!_is_run) return;

		_time_wait_ping += delta;

		if (_time_wait_ping > _ping_pong)
		{
			_is_run = false;

			close();
		}
	}
}
