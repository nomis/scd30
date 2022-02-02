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

#ifndef SCD30_NETWORK_H_
#define SCD30_NETWORK_H_

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif

#include <uuid/console.h>
#include <uuid/log.h>

namespace scd30 {

class Network {
public:
	static void connect();
	static void reconnect();
	static void disconnect();
	static void scan(uuid::console::Shell &shell);
	static void print_status(uuid::console::Shell &shell);

	void start();

private:
	void sta_mode_connected(const WiFiEventStationModeConnected &event);
	void sta_mode_disconnected(const WiFiEventStationModeDisconnected &event);
	void sta_mode_got_ip(const WiFiEventStationModeGotIP &event);
	void sta_mode_dhcp_timeout();

	static uuid::log::Logger logger_;
	::WiFiEventHandler sta_mode_connected_;
	::WiFiEventHandler sta_mode_disconnected_;
	::WiFiEventHandler sta_mode_got_ip_;
	::WiFiEventHandler sta_mode_dhcp_timeout_;
};

} // namespace scd30

#endif
