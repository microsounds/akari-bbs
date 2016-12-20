# akari-bbs — lightweight messageboard system
![powered] ![revision] ![database] ![license]

akari-bbs is an **_in-development_** lightweight messageboard system written in C!

You can see a running instance of akari-bbs: [here!](http://akaribbs.mooo.com/)

## thread-mode branch
**Latest working build is [master](https://github.com/microsounds/akari-bbs/tree/master) branch.**
**Commits in this branch may be squashed in the future.**

## Features
### No Registration
Like a traditional Futaba-like messageboard system, no registration is required to participate.

### Identity
Users can post anonymously, or they can maintain a persistent identity with a tripcode.

A tripcode is a static DES hash appended to your name. Just enter a # and a password after your name.
> `Akari#example` becomes _Akari !KtW6XcghiY_

Tripcodes can be considered a passive form of "user registration".

### Aggressive Content Turnover
Each board may host a limited number of active threads at once.

Replying to a thread bumps it to the front page, sinking all other threads by 1.
A hard limit on bumping ensures that long and stale threads cannot be bumped back to the front page.
Threads at the bottom of the index are slowly pruned off the board as new threads are created.
Pruned threads are archived for a limited time before being deleted.

This algorithm ensures high content turnover and ensures that all discussion is ephemeral in nature.

### Implemented Features
* Anonymous posting
  * Registration-free identity persistence is available.
* Tripcodes (DES crypt(3) a.k.a Futaba-style)
* Chronological order threaded discussion
* Self-contained discussion boards for different topics
* Aggressive content turnover algorithm
* Blockquotes and post quoting
* Formatting markup for spoilers and code blocks
* UTF-8 aware input sanitation
* User flooding deterrance

### Warning
This project is in alpha state and is not feature complete. Functionality may change at any time.

## Required
* lighttpd
* sqlite3

## Dependencies
* libcrypt (POSIX)
* libsqlite3

## Deployment
1. Install `lighttpd`, `sqlite3`, and `libsqlite3-dev`.
2. Configure lighttpd by editing `/etc/lighttpd/lighttpd.conf` to include the contents of `server.conf`.
  * On a default installation, you can just `cat server.conf >> /etc/lighttpd/lighttpd.conf` as root.
3. Copy repo to your designated `server.document-root` location.
  * Default location is `/var/www`.
4. Build project with `make release`.
5. Initialize database and give `www-data` write permission by running `./init.sh` as root.
6. Enable and start Lighttpd using `service` or `systemctl`, depending on your distro.
7. Confirm that everything is working by visiting `localhost` in your browser.
  * If posting doesn't work, it means `www-data` is not the owner of the database file.

## License
Copyright (C) 2016 microsounds <<https://github.com/microsounds>>

akari-bbs is free software, released under the terms of the GNU General Public License v3 or later.

See [`include/global.h`](include/global.h) for full copyright or [`LICENSE`](LICENSE) for license information.

![akarin~]

[powered]: https://img.shields.io/badge/powered_by-akari--bbs-646464.svg?colorA=DC8B9A&style=flat-square
[revision]: https://img.shields.io/badge/revision-14-646464.svg?colorA=D5B2FE&style=flat-square
[database]: https://img.shields.io/badge/database-v4-646464.svg?colorA=B3AFFF&style=flat-square
[license]: https://img.shields.io/badge/license-GPLv3-646464.svg?colorA=AFD3FF&style=flat-square
[akarin~]: http://i.imgur.com/fOCh5UZ.gif
