# siftsearch

siftsearch is an image search tool utilizing OpenSIFT. Using SIFT it
will even find images heavily modified (parts replaced, rotated,
skewed and so on).

The modus operandi of this tool is to create a database from a given
set of images and then match images against that database.

[![Travis Build](https://api.travis-ci.org/enkore/j4-dmenu-desktop.png)](https://travis-ci.org/enkore/j4-dmenu-desktop/)


## Build requirements

* Compiler with basic C++11 support (GCC 4.8 or later required, Clang works, too)
* CMake
* OpenCV
* OpenSIFT
* Boost

Building is the usual cmake/make thingy:

    cmake .
    make
    sudo make install

## Invocation

Usage:


	siftsearch index [--db="sift.db"] [<directory>]
	siftsearch search [--db="sift.db"] <file>
	siftsearch --help

Subcommands:

	index [<directory>]
		Create or update a database from all images in the given directory, recursing subdirectories.
		<directory> defaults to the current working directory.

	search
		Match <file> against the database

Options:

	--db="sift.db"
		Selects the database to use, default is sift.db in the working directory

