/*
 * scd30 - SCD30 Monitor
 * Copyright 2022  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCD30_CONSOLE_H_
#define SCD30_CONSOLE_H_

#include <uuid/console.h>

#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif

#include <memory>
#include <string>
#include <vector>

#ifdef LOCAL
# undef LOCAL
#endif

namespace scd30 {

enum CommandFlags : unsigned int {
	USER = 0,
	ADMIN = (1 << 0),
	LOCAL = (1 << 1),
};

enum ShellContext : unsigned int {
	MAIN = 0,
};

class SCD30Shell: virtual public uuid::console::Shell {
public:
	~SCD30Shell() override = default;

	virtual std::string console_name() = 0;

protected:
	SCD30Shell();

	static std::shared_ptr<uuid::console::Commands> commands_;

	void started() override;
	void display_banner() override;
	std::string hostname_text() override;
	std::string prompt_suffix() override;
	void end_of_transmission() override;
	void stopped() override;
};

class SCD30StreamConsole: public uuid::console::StreamConsole, public SCD30Shell {
public:
	SCD30StreamConsole(Stream &stream, bool local);
	SCD30StreamConsole(Stream &stream, const IPAddress &addr, uint16_t port);
	~SCD30StreamConsole() override;

	std::string console_name();

private:
	static std::vector<bool> ptys_;

	std::string name_;
	size_t pty_;
	IPAddress addr_;
	uint16_t port_;
};

} // namespace scd30

#endif
