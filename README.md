# siftsearch

siftsearch is an image search tool utilizing OpenSIFT. Using SIFT it
will even find images heavily modified (parts replaced, rotated,
skewed and so on).

The modus operandi of this tool is to create a database from a given
set of images and then match images against that database.

<!-- [![Travis Build](https://api.travis-ci.org/enkore/j4-dmenu-desktop.png)](https://travis-ci.org/enkore/j4-dmenu-desktop/) -->


## Build requirements

* C Compiler with OpenMP support
* CMake
* OpenCV
* OpenSIFT
* GDBM

Building is the usual cmake/make thingy:

    cmake .
    make
    sudo make install

## Invocation

    siftsearch
    Yet another image search tool
    Copyright (c) 2014 Marian Beermann, GPLv3 license
    
    Usage:
            siftsearch --help
            siftsearch <options> --index <directory> [--clean]
            siftsearch <options> [--exec <command>] <file> ...
            siftsearch <options> --dump
    
    Options:
        --db <file>
            Set database file to use.
        --exec <command>
            Execute <command> with absolute paths to found files sorted by ascending
            match percentage.
        --verbose
            Enable verbose output.
    
    Commands:
        --index <directory>
            Create or update database from all images in the given directory
            , recursing subdirectories.
        --clean
            Discard existing database contents, if any. Not effective without
            --index.
        --help
            Display this help message.
        --dump
            List all indexed files and exit.
    
    Standard Output:
    If standard output is connected to a terminal, detailed match information is
    printed for each file (number of matches and percentage). If standard output
    is not connected to a terminal only the file name is printed.
    Error messages and --verbose information is always printed to standard error.
