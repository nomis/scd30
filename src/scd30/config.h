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

#ifndef SCD30_CONFIG_H_
#define SCD30_CONFIG_H_

#include <ArduinoJson.hpp>

#include <string>

#include <uuid/log.h>

namespace scd30 {

class Config {
public:
	Config(bool mount = true);
	~Config() = default;

	std::string admin_password() const;
	void admin_password(const std::string &admin_password);

	std::string hostname() const;
	void hostname(const std::string &hostname);

	std::string wifi_ssid() const;
	void wifi_ssid(const std::string &wifi_ssid);

	std::string wifi_password() const;
	void wifi_password(const std::string &wifi_password);

	std::string syslog_host() const;
	void syslog_host(const std::string &syslog_host);

	uuid::log::Level syslog_level() const;
	void syslog_level(uuid::log::Level syslog_level);

	unsigned long syslog_mark_interval() const;
	void syslog_mark_interval(unsigned long syslog_mark_interval);

	bool ota_enabled() const;
	void ota_enabled(bool ota_enabled);

	std::string ota_password() const;
	void ota_password(const std::string &ota_password);

	bool sensor_automatic_calibration() const;
	void sensor_automatic_calibration(bool sensor_automatic_calibration);

	unsigned long sensor_temperature_offset() const;
	void sensor_temperature_offset(unsigned long sensor_temperature_offset);

	unsigned long sensor_altitude_compensation() const;
	void sensor_altitude_compensation(unsigned long sensor_altitude_compensation);

	unsigned long sensor_measurement_interval() const;
	void sensor_measurement_interval(unsigned long sensor_measurement_interval);

	unsigned long sensor_ambient_pressure() const;
	void sensor_ambient_pressure(unsigned long sensor_ambient_pressure);

	unsigned long take_measurement_interval() const;
	void take_measurement_interval(unsigned long take_measurement_interval);

	bool report_enabled() const;
	void report_enabled(bool report_enabled);

	unsigned long report_threshold() const;
	void report_threshold(unsigned long report_threshold);

	std::string report_url() const;
	void report_url(const std::string &report_url);

	std::string report_username() const;
	void report_username(const std::string &report_username);

	std::string report_password() const;
	void report_password(const std::string &report_password);

	std::string report_sensor_name() const;
	void report_sensor_name(const std::string &report_sensor_name);

	void commit();
	void umount();

private:
	static constexpr size_t BUFFER_SIZE = 4096;

	static uuid::log::Logger logger_;

	static bool mounted_;
	static bool unavailable_;
	static bool loaded_;

	static std::string admin_password_;
	static std::string hostname_;
	static std::string wifi_password_;
	static std::string wifi_ssid_;
	static std::string syslog_host_;
	static uuid::log::Level syslog_level_;
	static unsigned long syslog_mark_interval_;
	static bool ota_enabled_;
	static std::string ota_password_;
	static bool sensor_automatic_calibration_;
	static unsigned long sensor_temperature_offset_;
	static unsigned long sensor_altitude_compensation_;
	static unsigned long sensor_measurement_interval_;
	static unsigned long sensor_ambient_pressure_;
	static unsigned long take_measurement_interval_;
	static bool report_enabled_;
	static unsigned long report_threshold_;
	static std::string report_url_;
	static std::string report_username_;
	static std::string report_password_;
	static std::string report_sensor_name_;

	bool read_config(const std::string &filename, bool load = true);
	void read_config(const ArduinoJson::JsonDocument &doc);
	bool write_config(const std::string &filename);
	void write_config(ArduinoJson::JsonDocument &doc);
};

} // namespace scd30

#endif
