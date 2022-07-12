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

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <uuid/console.h>
#include <uuid/log.h>

#include "scd30/app.h"
#include "app/config.h"
#include "app/console.h"

using ::uuid::flash_string_vector;
using ::uuid::console::Commands;
using ::uuid::console::Shell;
using LogLevel = ::uuid::log::Level;
using LogFacility = ::uuid::log::Facility;

using CommandFlags = ::app::CommandFlags;
using Config = ::app::Config;
using ShellContext = ::app::ShellContext;

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = string_literal;
#define MAKE_PSTR_WORD(string_name) MAKE_PSTR(string_name, #string_name)
#define F_(string_name) FPSTR(__pstr__##string_name)

namespace scd30 {

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wunused-const-variable"
MAKE_PSTR_WORD(altitude)
MAKE_PSTR_WORD(ambient)
MAKE_PSTR_WORD(calibrate)
MAKE_PSTR_WORD(compensation)
MAKE_PSTR_WORD(interval)
MAKE_PSTR_WORD(measurement)
MAKE_PSTR_WORD(name)
MAKE_PSTR_WORD(off)
MAKE_PSTR_WORD(offset)
MAKE_PSTR_WORD(on)
MAKE_PSTR_WORD(password)
MAKE_PSTR_WORD(pressure)
MAKE_PSTR_WORD(reading)
MAKE_PSTR_WORD(report)
MAKE_PSTR_WORD(sensor)
MAKE_PSTR_WORD(set)
MAKE_PSTR_WORD(show)
MAKE_PSTR_WORD(temperature)
MAKE_PSTR_WORD(threshold)
MAKE_PSTR_WORD(username)
MAKE_PSTR_WORD(url)
MAKE_PSTR(altitude_optional, "[altitude above sea level in m]")
MAKE_PSTR(count_optional, "[count]")
MAKE_PSTR(name_optional, "[name]")
MAKE_PSTR(new_password_prompt1, "Enter new password: ")
MAKE_PSTR(new_password_prompt2, "Retype new password: ")
MAKE_PSTR(ppm_mandatory, "<CO₂ concentration in ppm>")
MAKE_PSTR(pressure_optional, "[pressure in mbar]")
MAKE_PSTR(seconds_optional, "[seconds]")
MAKE_PSTR(temperature_optional, "[temperature in °C]")
MAKE_PSTR(url_optional, "[url]")
#pragma GCC diagnostic pop

static inline App &to_app(Shell &shell) {
	return static_cast<App&>(dynamic_cast<app::AppShell&>(shell).app_);
}

static inline void setup_commands(std::shared_ptr<Commands> &commands) {
	#define NO_ARGUMENTS std::vector<std::string>{}

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(sensor), F_(name)},
			flash_string_vector{F_(name_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.report_sensor_name(arguments.front());
			config.commit();
			to_app(shell).config_report();
		}
		shell.printfln(F("Report sensor name = %s"), config.report_sensor_name().c_str());
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(on)},
			[=] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		config.report_enabled(true);
		config.commit();
		to_app(shell).config_report();
		shell.println(F("Reporting enabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(off)},
			[=] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		config.report_enabled(false);
		config.commit();
		to_app(shell).config_report();
		shell.println(F("Reporting disabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(threshold)},
			flash_string_vector{F_(count_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			unsigned long value = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%lu", &value);

			if (ret < 1) {
				shell.println(F("Invalid value"));
				return;
			}

			config.report_threshold(value);
			config.commit();
			to_app(shell).config_report();
		}
		shell.printfln(F("Report threshold = %u"), config.report_threshold());
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(username)},
			flash_string_vector{F_(name_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.report_username(arguments.front());
			config.commit();
			to_app(shell).config_report();
		}
		shell.printfln(F("Report username = %s"), config.report_username().c_str());
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(report), F_(url)},
			flash_string_vector{F_(url_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.report_url(arguments.front());
			config.commit();
			to_app(shell).config_report();
		}
		shell.printfln(F("Report URL = %s"), config.report_url().c_str());
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(report), F_(password)},
			[=] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.enter_password(F_(new_password_prompt1), [] (Shell &shell, bool completed, const std::string &password1) {
				if (completed) {
					shell.enter_password(F_(new_password_prompt2), [password1] (Shell &shell, bool completed, const std::string &password2) {
						if (completed) {
							if (password1 == password2) {
								Config config;
								config.report_password(password2);
								config.commit();
								if (config.report_password().empty()) {
									shell.println(F("Cleared report password"));
								} else {
									shell.println(F("Set report password"));
								}
								to_app(shell).config_report();
							} else {
								shell.println(F("Passwords do not match"));
							}
						}
					});
				}
			});
	});

	auto sensor_altitude_compensation = [] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		unsigned long value = config.sensor_altitude_compensation();

		shell.printfln(F("Altitude compensation: %lum"), value);
	};

	auto sensor_ambient_pressure = [] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		unsigned long value = config.sensor_ambient_pressure();

		if (value != 0) {
			shell.printfln(F("Ambient pressure compensation: %lu mbar"), value);
		} else {
			shell.println(F("Ambient pressure compensation: disabled"));
		}
	};

	auto sensor_measurement_interval = [] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		unsigned long value = config.sensor_measurement_interval();

		shell.printfln(F("Measurement interval: %lus"), value);
	};

	auto sensor_reading_interval = [] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		unsigned long value = config.take_measurement_interval();

		if (value != 0) {
			shell.printfln(F("Reading interval: %lus"), value);
		} else {
			shell.println(F("Readings disabled"));
		}
	};

	auto sensor_temperature_offset = [] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		unsigned long value = config.sensor_temperature_offset();

		shell.printfln(F("Temperature offset: %lu.%02lu°C"), value / 100, value % 100);
	};

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(sensor)},
			[=] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.printfln(F("Sensor firmware: %s"), to_app(shell).sensor().firmware_version().c_str());
		sensor_altitude_compensation(shell, NO_ARGUMENTS);
		sensor_ambient_pressure(shell, NO_ARGUMENTS);
		sensor_measurement_interval(shell, NO_ARGUMENTS);
		sensor_temperature_offset(shell, NO_ARGUMENTS);
		shell.println();
		shell.printfln(F("Temperature:       %.2f°C"), to_app(shell).sensor().temperature_c());
		shell.printfln(F("Relative humidity: %.2f%%"), to_app(shell).sensor().relative_humidity_pc());
		shell.printfln(F("CO₂:               %.2f ppm"), to_app(shell).sensor().co2_ppm());
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, CommandFlags::ADMIN,
		flash_string_vector{F_(sensor), F_(altitude), F_(compensation)}, sensor_altitude_compensation);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(altitude), F_(compensation)},
			flash_string_vector{F_(altitude_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (!arguments.empty()) {
			unsigned long value = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%lu", &value);

			if (ret < 1) {
				shell.println(F("Invalid value"));
				return;
			}

			config.sensor_altitude_compensation(value);
			config.commit();
			to_app(shell).config_sensor({Operation::CONFIG_ALTITUDE_COMPENSATION});
		}

		sensor_altitude_compensation(shell, NO_ARGUMENTS);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, CommandFlags::ADMIN,
		flash_string_vector{F_(sensor), F_(ambient), F_(pressure)}, sensor_ambient_pressure);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(ambient), F_(pressure)},
			flash_string_vector{F_(pressure_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (!arguments.empty()) {
			unsigned long value = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%lu", &value);

			if (ret < 1) {
				shell.println(F("Invalid value"));
				return;
			}

			config.sensor_ambient_pressure(value);
			config.commit();
			to_app(shell).config_sensor({Operation::CONFIG_AMBIENT_PRESSURE});
		}

		sensor_ambient_pressure(shell, NO_ARGUMENTS);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(calibrate)},
			flash_string_vector{F_(ppm_mandatory)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		unsigned long value = 0;
		int ret = std::sscanf(arguments.front().c_str(), "%lu", &value);

		if (ret < 1 || value < Sensor::MINIMUM_CALIBRATION_PPM || value > Sensor::MAXIMUM_CALIBRATION_PPM) {
			shell.println(F("Invalid value"));
			return;
		}

		to_app(shell).calibrate_sensor(value);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, CommandFlags::ADMIN,
		flash_string_vector{F_(sensor), F_(measurement), F_(interval)}, sensor_measurement_interval);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(measurement), F_(interval)},
			flash_string_vector{F_(seconds_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (!arguments.empty()) {
			unsigned long value = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%lu", &value);

			if (ret < 1) {
				shell.println(F("Invalid value"));
				return;
			}

			config.sensor_measurement_interval(value);
			config.commit();
			to_app(shell).config_sensor({Operation::CONFIG_CONTINUOUS_MEASUREMENT});
		}

		sensor_measurement_interval(shell, NO_ARGUMENTS);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, CommandFlags::ADMIN,
		flash_string_vector{F_(sensor), F_(reading), F_(interval)}, sensor_reading_interval);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(reading), F_(interval)},
			flash_string_vector{F_(seconds_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (!arguments.empty()) {
			unsigned long value = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%lu", &value);

			if (ret < 1) {
				shell.println(F("Invalid value"));
				return;
			}

			config.take_measurement_interval(value);
			config.commit();
			to_app(shell).config_sensor({Operation::TAKE_MEASUREMENT});
		}

		sensor_reading_interval(shell, NO_ARGUMENTS);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, CommandFlags::ADMIN,
		flash_string_vector{F_(sensor), F_(temperature), F_(offset)}, sensor_temperature_offset);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sensor), F_(temperature), F_(offset)},
			flash_string_vector{F_(temperature_optional)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (!arguments.empty()) {
			float fvalue = 0;
			int ret = std::sscanf(arguments[0].c_str(), "%f", &fvalue);
			long lvalue = std::lroundf(fvalue * 100);

			if (ret < 1 || !std::isfinite(fvalue) || lvalue < 0 || lvalue > UINT16_MAX) {
				shell.println(F("Invalid value"));
				return;
			}

			config.sensor_temperature_offset(lvalue);
			config.commit();
			to_app(shell).config_sensor({Operation::CONFIG_TEMPERATURE_OFFSET});
		}

		sensor_temperature_offset(shell, NO_ARGUMENTS);
	});
}

} // namespace scd30

namespace app {

void setup_commands(std::shared_ptr<Commands> &commands) {
	scd30::setup_commands(commands);
}

} // namespace scd30
