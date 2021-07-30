# Winestreamproxy

Forwards data between a Windows named pipe and a Unix domain socket. Primarily made for allowing games that run under
Wine to connect to native Linux Discord and show rich presence information in the user's status.

**"Top directory" in this file refers to the directory where the Makefile for Winestreamproxy is located.**

## Prerequisites

To compile, you need a working Wine, GCC, system headers, a shell, and make.

To run the program, you need a working Wine, libc, and a shell.

## Compiling

To compile Winestreamproxy using the default build directories, run
```
sh ./build-64.sh
```
in the top directory.

To remove all build results and temporary files, run
```
make clean
```
in the top directory.

To compile a 32-bit version, do
```
sh ./build-32.sh
```
instead.

## System-wide installation

**Compile the program before attempting to install it.**

To install Winestreamproxy into system directories, run
```sh
make install
```
in the top directory as root. This will install the program files into directories below /usr/local.

To uninstall, simply run
```sh
make uninstall
```
in the top directory as root.

## Usage

If you're using a release tarball, the below commands all need to be run in the directory where the tarball was
extracted. Alternatively the script paths in the commands can be expanded to the full paths of the scripts.

Use the commands marked with `(system)` for system-wide installations and the commands marked with `(tarball)` for
the release tarballs.

After installing the program, it can be launched by simply typing
```sh
 (system) winestreamproxy
(tarball) ./start.sh
```
into a terminal (assuming your `$PATH` is set up correctly).

There are some command line parameters. You can find out more about these by running
```sh
 (system) winestreamproxy --help
(tarball) ./start.sh --help
```
in a terminal.

To stop a running proxy, run
```sh
 (system) winestreamproxy-stop
(tarball) ./stop.sh
```
in a terminal.

If you want to run Winestreamproxy and another program (e.g. a game) in the same command (e.g. for use as the command
prefix in Lutris), use the included wrapper script. This script can be used by prepending
```sh
 (system) winestreamproxy-wrapper
(tarball) ./wrapper.sh
```
to the command to be run. Note that it is currently not possible to pass command line parameters to Winestreamproxy
through the wrapper script - if you need to do this, you will have to write your own wrapper script that includes
these parameters.

If you want to run the program in a different wineprefix than the default, you need to set the `$WINEPREFIX` environment
variable first.

Configuration files can be put in any of the following locations:

- &lt;extracted tarball&gt;/settings.conf
- /usr/local/lib/winestreamproxy/settings.conf
- Any of $XDG_CONFIG_DIRS/winestreamproxy/settings.conf
- $XDG_CONFIG_HOME/winestreamproxy/settings.conf
- winestreamproxy.conf in the working directory

See the default settings.conf for possible configuration options.

Once you have a working configuration, you can run
```sh
 (system) winestreamproxy-install
(tarball) ./install.sh
```
to install the Winestreamproxy service into a prefix. It will then be started every time the prefix is started.
After changing any settings you need to install the service again, otherwise it will continue to use the old settings.

To uninstall the service, run
```sh
 (system) winestreamproxy-uninstall
(tarball) ./uninstall.sh
```
