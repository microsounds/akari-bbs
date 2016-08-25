#!/usr/bin/env bash

# initializes database
# you must run this before starting

DBDIR=db

mkdir -p $DBDIR
cat sql/create_database.sql | sqlite3 $DBDIR/database.sqlite3
sudo chown www-data $DBDIR
sudo chown www-data $DBDIR/database.sqlite3
