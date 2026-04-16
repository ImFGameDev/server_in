#ifndef IN_SESSION_WEB_H
#define IN_SESSION_WEB_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>
#include <sessions/i_session.h>
#include <returnable/hash_events_getter.h>

namespace main_player::logic::connection
{
	class in_web_session : public i_session
	{
	private:
		std::vector<std::tuple<std::uint8_t, std::string, std::function<void(bool)>>> _write_queue;
		main_player::core::actions::hash_events_getter<std::uint8_t, const std::string&>* _event;
		boost::beast::websocket::stream<boost::asio::ip::tcp::socket>* _ws;
		std::function<void()> _close_callback;
		std::mutex _write_mutex;
		std::uint16_t _buffer_size;
		float _time_wait_ping;
		std::atomic<bool> _is_closing;
		bool _is_writing;
		bool _is_run;

		void process_write_queue();

		void read_message();

		void send_internal(const std::uint8_t& tag, const std::string& json, std::function<void(bool)> callback);

		void close();

	public:
		explicit in_web_session(boost::asio::ip::tcp::socket* socket);

		~in_web_session() override;

		void send(const std::uint8_t& tag, const std::string& json) override;

		void send(const std::uint8_t& tag, const std::string& json, std::function<void(bool)> on_send) override;

		void set_listener_close(const std::function<void()>& callback) override;

		void add_listener(const std::uint8_t& tag, std::function<void(const std::string&)> func) override;

		void remove_listener(const std::uint8_t& tag) override;

		void tick(const float& delta) override;

		bool is_closed() override;
	};
}

#endif
