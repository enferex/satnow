satnow: Collect satellite data and calculate look angles.
==========================================================

satnow is a utility for collecting satellite position information and for
displaying satellite look angles.  This might be useful for those interested in
satellite observation.  The list of satellites is presented based on the
range from the user's latitude/longitude/altitude.  Closer things appear first.

satnow will create a database, by default `.satnow.sqlite3`, in your current
working directory. That database contains all of the collected TLE entries you
have specified while updating. You can override the default by using the `--db`
command line option.

satnow works on position information presented in the Two-Line Element
([TLE](https://en.wikipedia.org/wiki/Two-line_element_set)). satnow can
download the TLE data or load pre-existing data from a text file on disk.

Usage
-----
If you do not have TLE data, download some! (Use the `--update=<source file>`
option and pass it a path to a file that contains a list of URLs to TLE data).
In other words, the file argument to `--update` is just a newline delimited
list of files or URLs that have the actual TLE information.
Example: `./satnow --update=sources.txt`

When running the tool, pass in your latitude and longitude in decimal degrees:
(via the `--lat` and `--lon` command line options).

The `--update=<source file>` command line option is used to retrieve, store, and
update satellite position information.  This updates the database. The file
argument to `--update` must point to a text file that contains a list of file
paths and URLs. Each URL and path must be on its own line. Those paths and URLs
contain
the actual set of TLE data.  For example:
```
# Example source file for use with the --update option
/path/to/TLE.txt  # Some of my favorite TLE data
/path/to/TLE2.txt # More awesome TLE entries
http://some.example.com/noaa-tle.txt # A fictitious URL containing TLE data.
```
The (optional) `--gui` support is recommended, as it presents the data in a
clean manner that is easy to refresh.  The refresh of data means re-calculating
the satellite look angles at the current time (satellites move quickly so their
position changes predictably quick).

Building
--------
1. Create a separate build directory.
1. From the just created build directory, invoke cmake with the path to the
 satnow sources. `cmake <path-to-satnow sources`
1. Invoke `make` to automatically download and build the libsgp4 dependency.
This will also build satnow.  `make`

Dependencies
------------
* [libsgp4](https://github.com/dnwrnr/sgp4): [dnwr's](https://github.com/dnwrnr) awesome libsgp4 library.
This library provides some useful clock and TLE classes,  calculates look
angles, and performs other positioning magic via the
[SGP4](https://en.wikipedia.org/wiki/Simplified_perturbations_models) model.
*This library is downloaded and built automatically*
* [sqlite3](sqlite.org): Database library used for storing the collected TLE information.
* [curl](https://curl.haxx.se/libcurl/): Used to download TLE data from remote
sources specfied via a URL.
* [ncurses](https://www.gnu.org/software/ncurses/): (Optional) Curses library
used to render the `--gui` mode.

Resources
---------
* https://celestrak.com/: Tons of SGP orbital information and updated TLE
listings.
* https://tle.info/: Another source of TLE data.
* https://www.space-track.org/:
US Gov't contract developed site for producing TLE information.  Data is
provided by the Joint Space Operations Center.
* [TLESniff](https://github.com/enferex/TLESniff):  Much of the frontend of this
tool was based on functionality also presented in my TLESniff program.  If all
you want to do is collect and store TLE data, use that.

Contact
-------
Matt Davis: https://github.com/enferex
