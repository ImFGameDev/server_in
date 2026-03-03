#ifndef I_SESSION_H
#define I_SESSION_H

#include <cstdint>
#include <functional>
#include <string>

class i_session
{
public:
	virtual ~i_session() = default;

	virtual void send(const std::uint8_t& tag, const std::string& json) = 0;

	virtual void send(const std::uint8_t& tag, const std::string& json, std::function<void(bool)> on_send) = 0;

	virtual bool is_closed() = 0;

	virtual void set_listener_close(const std::function<void()>& callback) = 0;

	virtual void add_listener(const std::uint8_t& tag, std::function<void(const std::string&)> func) = 0;

	virtual void remove_listener(const std::uint8_t& tag) = 0;

	virtual void tick(const float& delta) = 0;
};

#endif
