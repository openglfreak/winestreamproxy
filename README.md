# winestreamproxy

Forwards data between a Windows named pipe and a Unix domain socket. Primarily made for allowing games that run under
Wine to connect to native Linux Discord and then show rich presence information in the user's status.

**"Top directory" in this file refers to the directory where the Makefile for winestreamproxy is located.**

## Prerequisites

To compile, you need a working Wine, GCC, system headers, a shell, and make.

To run the program, you need a working Wine, libc, and a shell.

## Compiling

To compile winestreamproxy using the default build directories, run
```
make
```
in the top directory.

To remove all build results and temporary files, run
```
make clean
```
in the top directory.

To compile a 32-bit version, do
```
make CFLAGS='-m32'
```
instead. If you have already compiled a 64-bit version, you need to clean the build directories first before making a
32-bit build, or choose different build directories by specifying the `OBJ` and `OUT` variables.

## System-wide installation

**Compile the program before attempting to install it.**

To install winestreamproxy into system directories, run
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

After installing the program, it can be launched by simply typing
```sh
winestreamproxy
```
into a terminal (assuming your `$PATH` is set up correctly).

There are some command line parameters. You can find out more about these by running
```sh
winestreamproxy --help
```
in a terminal.

If you want to run winestreamproxy and another program (e.g. a game) in the same command (e.g. for use as the command
prefix in Lutris), use the included wrapper script. This script can be used by prepending
```sh
winestreamproxy-wrapper
```
to the command to be run. Note that it is currently not possible to pass command line parameters to winestreamproxy
through the wrapper script - if you need to do this, you will have to write your own wrapper script that includes
these parameters.

If you want to run the program in a different wineprefix than the default, you need to set the `$WINEPREFIX` environment
variable first.

Configuration files can be put in any of the following locations:

- /usr/local/lib/winestreamproxy/settings.conf
- Any of $XDG_CONFIG_DIRS/winestreamproxy/settings.conf
- $XDG_CONFIG_HOME/winestreamproxy/settings.conf
- winestreamproxy.conf in the working directory

See the default settings.conf for possible configuration options.
