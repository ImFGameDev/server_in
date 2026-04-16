#include <iostream>

#include "../../include/sessions/web/in_web_session.h"
#include <returnable/hash_events_getter.h>
#include <debug_system.h>

#include "commands.h"

namespace main_player::logic::connection
{
	const int _ping_pong = 30;

	void in_web_session::read_message()
	{
		if (!_ws->is_open() || _is_closing)
		{
			close();
			return;
		}

		auto buffer = std::make_shared<boost::beast::flat_buffer>();

		_ws->async_read(*buffer, [this, buffer](boost::beast::error_code ec, std::size_t bytes_transferred)
		{
			if (ec)
			{
				if (ec != boost::beast::websocket::error::closed)
					main_player::core::debug::debug_system::error("web_session",
					                                              "WebSocket read failed: " + ec.message());

				close();
				return;
			}

			try
			{
				auto data = buffer->data();
				std::string message(static_cast<const char*>(data.data()), data.size());

				if (message.length() < 1)
				{
					main_player::core::debug::debug_system::error("web_session", "Empty WebSocket message");
					close();
					return;
				}

				std::uint8_t tag = static_cast<std::uint8_t>(message[0]);
				std::string json_data = message.substr(1);
				std::string log = "read: " + std::to_string(tag) + '/' + json_data;

				main_player::core::debug::debug_system::log("web_session", log);

				_event->invoke(tag, json_data);

				read_message();
			}
			catch (const std::exception& e)
			{
				main_player::core::debug::debug_system::error("web_session",
				                                              "Message processing error: " + std::string(e.what()));
				close();
			}
		});
	}

	void in_web_session::process_write_queue()
	{
		if (_write_queue.empty() || _is_closing || !_ws->is_open())
		{
			_is_writing = false;
			return;
		}

		_is_writing = true;

		auto [tag, json, callback] = _write_queue.front();

		_write_queue.erase(_write_queue.begin());

		try
		{
			std::string message;

			message.reserve(1 + json.length());
			message.push_back(static_cast<char>(tag));
			message.append(json);

			main_player::core::debug::debug_system::log("web_session", "send: " + std::to_string(tag) + '/' + json);

			_ws->async_write(boost::asio::buffer(message), [this, callback](boost::beast::error_code ec, std::size_t)
			{
				std::lock_guard<std::mutex> lock(_write_mutex);

				if (ec)
				{
					main_player::core::debug::debug_system::error("web_session",
					                                              "WebSocket write error: " + ec.message());
					if (callback) callback(false);
					close();
					return;
				}

				if (callback) callback(true);

				process_write_queue();
			});
		}
		catch (const std::exception& e)
		{
			std::lock_guard<std::mutex> lock(_write_mutex);
			main_player::core::debug::debug_system::error(
				"session", "Send preparation error: " + std::string(e.what()));
			if (callback) callback(false);
			close();
		}
	}

	void in_web_session::send_internal(const std::uint8_t& tag, const std::string& json,
	                                   std::function<void(bool)> callback)
	{
		if (!_ws->is_open() || _is_closing)
		{
			main_player::core::debug::debug_system::error("web_session", "Cannot send - WebSocket closed");
			if (callback) callback(false);
			return;
		}

		std::lock_guard<std::mutex> lock(_write_mutex);
		_write_queue.emplace_back(tag, json, callback);

		if (!_is_writing) process_write_queue();
	}

	void in_web_session::close()
	{
		if (_is_closing.exchange(true)) return;

		try
		{
			boost::beast::error_code ec;
			if (_ws && _ws->is_open())
			{
				_ws->close(boost::beast::websocket::close_code::normal, ec);
				if (ec)
					main_player::core::debug::debug_system::error("web_session",
					                                              "WebSocket close error: " + ec.message());
			}
		}
		catch (const std::exception& e)
		{
			main_player::core::debug::debug_system::error("web_session",
			                                              "Exception during close: " + std::string(e.what()));
		}

		if (_close_callback)
		{
			main_player::core::debug::debug_system::log_green("web_session", "close callback");

			_close_callback();
		}
		else main_player::core::debug::debug_system::error("web_session", "close callback null");
	}

	//Public:
	in_web_session::in_web_session(boost::asio::ip::tcp::socket* socket): _is_closing(false), _is_writing(false)
	{
		core::debug::debug_system::log("web_session", "in_web_session()");

		_ws = new boost::beast::websocket::stream<boost::asio::ip::tcp::socket>(std::move(*socket));
		_buffer_size = 4096;
		_event = new main_player::core::actions::hash_events_getter<std::uint8_t, const std::string&>();
		_time_wait_ping = 0;
		_is_run = true;

		try
		{
			_ws->accept();
			_ws->binary(true);

			read_message();

			_event->add_listener(SESSION_PING, [this](const std::string&)
			{
				_time_wait_ping = 0;

				send(SESSION_PING, "");
			});
		}
		catch (const std::exception& e)
		{
			std::cerr << "WebSocket accept error: " << e.what() << std::endl;
			close();
		}
	}

	in_web_session::~in_web_session()
	{
		core::debug::debug_system::log("web_session", "~in_web_session()");

		if (_ws)
		{
			try
			{
				boost::beast::error_code ec;
				if (_ws->is_open())
				{
					_ws->close(boost::beast::websocket::close_code::normal, ec);
					if (ec) std::cerr << "WebSocket close error: " << ec.message() << std::endl;
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "Exception during WebSocket close: " << e.what() << std::endl;
			}

			delete _ws;
			_ws = nullptr;
		}

		delete _event;
	}

	void in_web_session::set_listener_close(const std::function<void()>& callback)
	{
		_close_callback = callback;
	}

	void in_web_session::add_listener(const std::uint8_t& tag, std::function<void(const std::string&)> func)
	{
		_event->add_listener(tag, func);
	}

	void in_web_session::remove_listener(const std::uint8_t& tag)
	{
		_event->remove_listeners(tag);
	}

	void in_web_session::send(const std::uint8_t& tag, const std::string& json)
	{
		send_internal(tag, json, nullptr);
	}

	void in_web_session::send(const std::uint8_t& tag, const std::string& json, std::function<void(bool)> on_send)
	{
		send_internal(tag, json, on_send);
	}

	bool in_web_session::is_closed()
	{
		return !_is_run;
	}

	void in_web_session::tick(const float& delta)
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
