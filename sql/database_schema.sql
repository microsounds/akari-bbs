/*
 * database_schema.sql
 * akari-bbs database schema version 4
 */

/*

 == NOT YET IMPLEMENTED ==

CREATE TABLE moderators (
	id        INTEGER PRIMARY KEY,
	username  TEXT    NOT NULL,
	user_priv INTEGER NOT NULL,
	pass_hash TEXT    NOT NULL,
	session   TEXT    NOT NULL,
	expire    INTEGER NOT NULL
);

CREATE TABLE banned_users (
	ip        TEXT    NOT NULL,
	cookie    TEXT    NOT NULL,
	reason    TEXT    NOT NULL,
	expire    INTEGER NOT NULL,
);

 == NOT YET IMPLEMENTED ==

 */

CREATE TABLE boards (
	id        TEXT    PRIMARY KEY,
	name      TEXT    NOT NULL,
	desc      TEXT    NOT NULL,
	status    INTEGER NOT NULL /* board_status flags */
);

CREATE TABLE threads (
	board_id  TEXT    NOT NULL,
	post_id   INTEGER NOT NULL,
	status    INTEGER NOT NULL /* thread_status flags */
);

CREATE TABLE archived_threads (
	board_id  TEXT    NOT NULL,
	post_id   INTEGER NOT NULL
);

CREATE TABLE posts (
	board_id  TEXT    NOT NULL,
	parent_id INTEGER NOT NULL, /* 0 if parent */
	id        INTEGER NOT NULL,
	time      INTEGER NOT NULL,
	options   INTEGER NOT NULL, /* sage, etc. */
	user_priv INTEGER NOT NULL, /* user, mod, admin? */
	del_pass  TEXT    NOT NULL, /* user provided, cookie is default */
	ip        TEXT    NOT NULL,
	name      TEXT,
	trip      TEXT,
	comment   TEXT    NOT NULL
);
