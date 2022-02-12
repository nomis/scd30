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

#include "scd30/network.h"

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif
#if LWIP_IPV6
# include <lwip/netif.h>
# include <lwip/ip6_addr.h>
# include <lwip/dhcp6.h>
#endif

#include <functional>

#include "scd30/config.h"

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "wifi";

namespace scd30 {

uuid::log::Logger Network::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::KERN};

void Network::start() {
	WiFi.persistent(false);

	sta_mode_connected_ = WiFi.onStationModeConnected(std::bind(&Network::sta_mode_connected, this, std::placeholders::_1));
	sta_mode_disconnected_ = WiFi.onStationModeDisconnected(std::bind(&Network::sta_mode_disconnected, this, std::placeholders::_1));
	sta_mode_got_ip_ = WiFi.onStationModeGotIP(std::bind(&Network::sta_mode_got_ip, this, std::placeholders::_1));
	sta_mode_dhcp_timeout_ = WiFi.onStationModeDHCPTimeout(std::bind(&Network::sta_mode_dhcp_timeout, this));

	connect();
}

void Network::sta_mode_connected(const WiFiEventStationModeConnected &event) {
	logger_.info(F("Connected to %s (%02X:%02X:%02X:%02X:%02X:%02X) on channel %u"),
			event.ssid.c_str(),
			event.bssid[0], event.bssid[1], event.bssid[2], event.bssid[3], event.bssid[4], event.bssid[5],
			event.channel);
#if LWIP_IPV6
	// Disable this otherwise it makes a query for every single RA
	dhcp6_disable(netif_default);
#endif
}

void Network::sta_mode_disconnected(const WiFiEventStationModeDisconnected &event) {
	logger_.info(F("Disconnected from %s (%02X:%02X:%02X:%02X:%02X:%02X) reason=%d"),
			event.ssid.c_str(),
			event.bssid[0], event.bssid[1], event.bssid[2], event.bssid[3], event.bssid[4], event.bssid[5],
			event.reason);
}

void Network::sta_mode_got_ip(const WiFiEventStationModeGotIP &event) {
	logger_.info(F("Obtained IPv4 address %s/%s and gateway %s"),
			uuid::printable_to_string(event.ip).c_str(),
			uuid::printable_to_string(event.mask).c_str(),
			uuid::printable_to_string(event.gw).c_str());
}

void Network::sta_mode_dhcp_timeout() {
	logger_.warning(F("DHCPv4 timeout"));
}

void Network::connect() {
	Config config;

	WiFi.mode(WIFI_STA);

	if (!config.wifi_ssid().empty()) {
		WiFi.begin(config.wifi_ssid().c_str(), config.wifi_password().c_str());
	}
}

void Network::reconnect() {
	disconnect();
	connect();
}

void Network::disconnect() {
	WiFi.disconnect();
}

void Network::scan(uuid::console::Shell &shell) {
	int8_t ret = WiFi.scanNetworks(true);
	if (ret == WIFI_SCAN_RUNNING) {
		shell.println(F("Scanning for WiFi networks..."));

		shell.block_with([] (uuid::console::Shell &shell, bool stop) -> bool {
			int8_t ret = WiFi.scanComplete();

			if (ret == WIFI_SCAN_RUNNING) {
				return stop;
			} else if (ret == WIFI_SCAN_FAILED || ret < 0) {
				shell.println(F("WiFi scan failed"));
				return true;
			} else {
				shell.printfln(F("Found %u networks"), ret);
				shell.println();

				for (uint8_t i = 0; i < (uint8_t)ret; i++) {
					shell.printfln(F("%s (channel %u at %d dBm) %s"),
							WiFi.SSID(i).c_str(),
							WiFi.channel(i),
							WiFi.RSSI(i),
							WiFi.BSSIDstr(i).c_str());
				}

				WiFi.scanDelete();
				return true;
			}
		});
	} else {
		shell.println(F("WiFi scan failed"));
	}
}

void Network::print_status(uuid::console::Shell &shell) {
	switch (WiFi.status()) {
	case WL_IDLE_STATUS:
		shell.printfln(F("WiFi: idle"));
		break;

	case WL_NO_SSID_AVAIL:
		shell.printfln(F("WiFi: network not found"));
		break;

	case WL_SCAN_COMPLETED:
		shell.printfln(F("WiFi: network scan complete"));
		break;

	case WL_CONNECTED:
		{
			shell.printfln(F("WiFi: connected"));
			shell.println();

			shell.printfln(F("SSID: %s"), WiFi.SSID().c_str());
			shell.printfln(F("BSSID: %s"), WiFi.BSSIDstr().c_str());
			shell.printfln(F("RSSI: %d dBm"), WiFi.RSSI());
			shell.println();

			shell.printfln(F("MAC address: %s"), WiFi.macAddress().c_str());
			shell.printfln(F("Hostname: %s"), WiFi.hostname().c_str());
			shell.println();

			shell.printfln(F("IPv4 address: %s/%s"),
					uuid::printable_to_string(WiFi.localIP()).c_str(),
					uuid::printable_to_string(WiFi.subnetMask()).c_str());

			shell.printfln(F("IPv4 gateway: %s"),
					uuid::printable_to_string(WiFi.gatewayIP()).c_str());

			shell.printfln(F("IPv4 nameserver: %s"),
					uuid::printable_to_string(WiFi.dnsIP()).c_str());

#if LWIP_IPV6
			shell.println();
			for (size_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
				if (ip6_addr_isvalid(netif_ip6_addr_state(netif_default, i))) {
					shell.printfln(F("IPv6 address: %s"),
							uuid::printable_to_string(IPAddress(netif_ip_addr6(netif_default, i))).c_str());
				}
			}
#endif
		}
		break;

	case WL_CONNECT_FAILED:
		shell.printfln(F("WiFi: connection failed"));
		break;

	case WL_CONNECTION_LOST:
		shell.printfln(F("WiFi: connection lost"));
		break;

	case WL_DISCONNECTED:
		shell.printfln(F("WiFi: disconnected"));
		break;

	case WL_NO_SHIELD:
	default:
		shell.printfln(F("WiFi: unknown"));
		break;
	}
}

} // namespace scd30
