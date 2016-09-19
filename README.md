# akari-bbs
akari-bbs is a _**work-in-progress**_ lightweight messageboard system written in C!

### Implemented Features
* Anonymous posting
* Blockquotes and post quoting
* Cooldown timer
* UTF-8 aware input sanitation
* Usernames
* Tripcodes (DES crypt-based a.k.a Futaba-style)
  * Enter your password in the form "name#password" to generate a tripcode.
  * Tripcodes are a simple way to provide persistent identity without registration.

Server-side page response time currently clocks in at 0.8ms. (1/1250th of a second!)

You can see a running instance of akari-bbs: [here!](http://akaribbs.mooo.com/)

## Required
* lighttpd
* sqlite3

## Dependencies
* libcrypt (POSIX)
* libsqlite3

## Deployment
1. Install ```lighttpd```, ```sqlite3```, and ```libsqlite3-dev```.
2. In ```/etc/lighttpd/lighttpd.conf```, add the following lines:

   ```
   server.modules += "mod_cgi"
   cgi.assign = ( ".cgi"  => "" )
   ```
3. Copy repo to your selected ```server.document-root``` location.
4. Build project with ```make release```.
5. Initialize database and give ```www-data``` write permission by running ```./init.sh``` as root.
6. Enable and start Lighttpd using ```service``` or ```systemctl```, depending on your distro.
7. Check if everything went well by visiting ```localhost/akari-bbs/board.cgi``` in your browser.

## License
Copyright (C) 2016 microsounds <<https://github.com/microsounds>>

akari-bbs is free software, released under the terms of the GNU General Public License v3 or later.

See ```src/board.c``` for full copyright or ```LICENSE``` for license information.

![Akari!](http://i.imgur.com/fOCh5UZ.gif)
