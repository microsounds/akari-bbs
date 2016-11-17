#!/usr/bin/env sh

# initializes database and sets permissions
# you must run this before starting
# run it again anytime to reset DB permissions

DBDIR=db
DBFILE=database.sqlite3
DBSCHEMA=sql/database_schema.sql
DBPATH=$PWD/$DBDIR/$DBFILE
DBOWNER=www-data

if ! [ -x "$(command -v sqlite3)" ]
then
	echo "SQLite3 cannot be found or is not installed."
	exit 1
fi

if [ $(id -u) != 0 ]
then
	echo "You must run this script as root."
	exit 1
fi

if [ -f $DBDIR/$DBFILE ]
then
	echo "Database already exists at '$DBPATH'."
	echo "Permissions have been reset."
else
	mkdir -p $DBDIR
	cat $DBSCHEMA | sqlite3 $DBDIR/$DBFILE
	echo "Database initialized at '$DBPATH'."
fi

chown $DBOWNER $DBDIR
chown $DBOWNER $DBDIR/$DBFILE
exit 0
