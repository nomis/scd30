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

#include "scd30/config.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <IPAddress.h>

#include <cmath>
#include <string>
#include <vector>

#include <uuid/common.h>
#include <uuid/log.h>
#include <ArduinoJson.hpp>

#include "scd30/app.h"

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = string_literal;

namespace scd30 {

#define SCD30_CONFIG_DATA \
		SCD30_CONFIG_SIMPLE(std::string, "", admin_password, "", "") \
		SCD30_CONFIG_SIMPLE(std::string, "", hostname, "", "") \
		SCD30_CONFIG_SIMPLE(std::string, "", wifi_ssid, "", "") \
		SCD30_CONFIG_SIMPLE(std::string, "", wifi_password, "", "") \
		SCD30_CONFIG_CUSTOM(std::string, "", syslog_host, "", "") \
		SCD30_CONFIG_ENUM(uuid::log::Level, "", syslog_level, "", uuid::log::Level::OFF) \
		SCD30_CONFIG_PRIMITIVE(unsigned long, "", syslog_mark_interval, "", 0) \
		SCD30_CONFIG_PRIMITIVE(bool, "", ota_enabled, "", true) \
		SCD30_CONFIG_SIMPLE(std::string, "", ota_password, "", "")

/* Create member data and flash strings */
#define SCD30_CONFIG_SIMPLE SCD30_CONFIG_GENERIC
#define SCD30_CONFIG_CUSTOM SCD30_CONFIG_GENERIC
#define SCD30_CONFIG_PRIMITIVE SCD30_CONFIG_GENERIC
#define SCD30_CONFIG_ENUM SCD30_CONFIG_GENERIC
#define SCD30_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name##_; \
		MAKE_PSTR(__name, __key_prefix #__name __key_suffix)
SCD30_CONFIG_DATA
#undef SCD30_CONFIG_GENERIC
#undef SCD30_CONFIG_ENUM

void Config::read_config(const ArduinoJson::JsonDocument &doc) {
#define SCD30_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__name(doc[FPSTR(__pstr__##__name)] | __read_default, ##__VA_ARGS__);
#define SCD30_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__name(static_cast<__type>(doc[FPSTR(__pstr__##__name)] | static_cast<int>(__read_default)), ##__VA_ARGS__);
	SCD30_CONFIG_DATA
#undef SCD30_CONFIG_GENERIC
#undef SCD30_CONFIG_ENUM
}

void Config::write_config(ArduinoJson::JsonDocument &doc) {
#define SCD30_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		doc[FPSTR(__pstr__##__name)] = __name();
#define SCD30_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		doc[FPSTR(__pstr__##__name)] = static_cast<int>(__name());
	SCD30_CONFIG_DATA
#undef SCD30_CONFIG_GENERIC
#undef SCD30_CONFIG_PRIMITIVE
#undef SCD30_CONFIG_ENUM
}

#undef SCD30_CONFIG_SIMPLE
#undef SCD30_CONFIG_PRIMITIVE
#undef SCD30_CONFIG_CUSTOM

/* Create getters/setters for simple config items */
#define SCD30_CONFIG_SIMPLE(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		} \
		void Config::__name(const __type &__name) { \
			__name##_ = __name; \
		}
/* Create getters/setters for primitive config items */
#define SCD30_CONFIG_ENUM SCD30_CONFIG_PRIMITIVE
#define SCD30_CONFIG_PRIMITIVE(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		} \
		void Config::__name(__type __name) { \
			__name##_ = __name; \
		}

/* Create getters for config items with custom setters */
#define SCD30_CONFIG_CUSTOM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		}

SCD30_CONFIG_DATA

#undef SCD30_CONFIG_SIMPLE
#undef SCD30_CONFIG_PRIMITIVE
#undef SCD30_CONFIG_CUSTOM
#undef SCD30_CONFIG_ENUM

static const char __pstr__config_filename[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "/config.msgpack";
static const char __pstr__config_backup_filename[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "/config.msgpack~";

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "config";
uuid::log::Logger Config::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};

bool Config::mounted_ = false;
bool Config::unavailable_ = false;
bool Config::loaded_ = false;

Config::Config() {
	if (!unavailable_ && !mounted_) {
		logger_.info(F("Mounting LittleFS filesystem"));
		if (LittleFS.begin()) {
			logger_.info(F("Mounted LittleFS filesystem"));
			mounted_ = true;
		} else {
			logger_.alert(F("Unable to mount LittleFS filesystem"));
			unavailable_ = true;
		}
	}

	if (mounted_ && !loaded_) {
		if (read_config(uuid::read_flash_string(FPSTR(__pstr__config_filename)))
				|| read_config(uuid::read_flash_string(FPSTR(__pstr__config_backup_filename)))) {
			loaded_ = true;
		}
	}

	if (!loaded_) {
		logger_.err(F("Config failure, using defaults"));
		read_config(ArduinoJson::StaticJsonDocument<0>());
		loaded_ = true;
	}
}

void Config::syslog_host(const std::string &syslog_host) {
	IPAddress addr;

	if (addr.fromString(syslog_host.c_str())) {
		syslog_host_= syslog_host;
	} else {
		syslog_host_.clear();
	}
}

void Config::commit() {
	if (mounted_) {
		std::string filename = uuid::read_flash_string(FPSTR(__pstr__config_filename));
		std::string backup_filename = uuid::read_flash_string(FPSTR(__pstr__config_backup_filename));

		if (write_config(filename)) {
			if (read_config(filename, false)) {
				write_config(backup_filename);
			}
		}
	}
}

bool Config::read_config(const std::string &filename, bool load) {
	logger_.info(F("Reading config file %s"), filename.c_str());
	File file = LittleFS.open(filename.c_str(), "r");
	if (file) {
		ArduinoJson::DynamicJsonDocument doc(BUFFER_SIZE);

		auto error = ArduinoJson::deserializeMsgPack(doc, file);
		if (error) {
			logger_.err(F("Failed to parse config file %s: %s"), filename.c_str(), error.c_str());
			return false;
		} else {
			if (load) {
				logger_.info(F("Loading config from file %s"), filename.c_str());
				read_config(doc);
			}
			return true;
		}
	} else {
		logger_.err(F("Config file %s does not exist"), filename.c_str());
		return false;
	}
}

bool Config::write_config(const std::string &filename) {
	logger_.info(F("Writing config file %s"), filename.c_str());
	File file = LittleFS.open(filename.c_str(), "w");
	if (file) {
		ArduinoJson::DynamicJsonDocument doc(BUFFER_SIZE);

		write_config(doc);

		ArduinoJson::serializeMsgPack(doc, file);

		if (file.getWriteError()) {
			logger_.alert(F("Failed to write config file %s: %u"), filename.c_str(), file.getWriteError());
			return false;
		} else {
			return true;
		}
	} else {
		logger_.alert(F("Unable to open config file %s for writing"), filename.c_str());
		return false;
	}
}

} // namespace scd30
