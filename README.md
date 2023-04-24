# Modbus Watchdog - Akita

Akita is a Modbus Watchdog daemon that monitors a specified Modbus register for changes. If the register value does not change within the specified time interval, Akita will execute a series of shell scripts located in a specified directory.

## Features

- Monitors a specific Modbus register for changes
- Configurable timeout for register value updates
- Executes shell scripts in alphabetical order if the register value does not change in time
- Resilient to Modbus server downtime and connection issues
- Configurable through a simple text-based configuration file


## Installation

To compile the Akita Modbus Watchdog, you will need the following libraries:

- libmodbus

First, clone the repository:

```bash
git clone https://github.com/ganehag/akita.git
```

Change to the Akita directory:

Run the following commands to compile the Akita daemon:

```bash
cd akita
```

Run the following commands to generate the configure script and compile the Akita daemon:

```bash
./autogen.sh
./configure
make
```

To install Akita and its configuration files on your system, run:

```bash
sudo make install
```

By default, the Akita binary will be installed in `/usr/bin`, and the configuration files will be placed in `/etc/akita`. You can modify the installation paths by providing custom options to the `./configure` command. For example:

```bash
./configure --prefix=/usr/local --sysconfdir=/etc
```

This command will install the Akita binary in `/usr/local/bin` and the configuration files in `/etc/akita`.

## Configuration

Akita can be configured using a text-based configuration file. By default, the configuration file should be located at `/etc/akita/akita.conf`.

Here's an example configuration file:

```text
host=127.0.0.1
port=502
slave_id=1
register_address=0
interval=600
timeout=1800
script_dir=/etc/akita/scripts.d
run_on_exit=true
```

Options:

- `host`: The IP address or hostname of the Modbus server
- `port`: The port number of the Modbus server
- `slave_id`: The Modbus slave ID
- `register_address`: The address of the Modbus register to monitor
- `interval`: The interval (in seconds) between checking the register for updates
- `timeout`: The maximum duration (in seconds) before triggering the shell scripts if the register value does not change
- `script_dir`: The directory containing the shell scripts to be executed when the watchdog is triggered


## Usage

To run the Akita Modbus Watchdog, simply execute the compiled binary:


You can run the Akita daemon in the background using the following command:

```bash
nohup ./akita -c /etc/akita/akita.conf &
```

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3). See the [LICENSE](LICENSE) file for more information.

