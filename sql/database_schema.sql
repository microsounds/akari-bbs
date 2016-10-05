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
	parent_id INTEGER NOT NULL,
	id        INTEGER NOT NULL,
	time      INTEGER NOT NULL,
	options   INTEGER NOT NULL, /* sage, etc. */
	user_priv INTEGER NOT NULL, /* user, mod, admin? */
	del_pass  TEXT    NOT NULL, /* user provided, cookie is default */
	ip        TEXT    NOT NULL,
	name      TEXT,
	trip      TEXT,
	subject   TEXT,
	comment   TEXT    NOT NULL
);

INSERT INTO boards VALUES
("test", "Dummy Board", "Dummy board for feature testing.", 0),
("meta", "Akari-BBS Discussion", "Meta Discussion goes here.", 0);

INSERT INTO threads VALUES ("test", 1, 0);

INSERT INTO posts VALUES
("test", 1, 1, 1471893064, 0, 1, "dummy", "192.168.1.1", NULL, NULL, NULL, "This is a sample comment!"),
("test", 1, 2, 1371293064, 0, 1, "dummy", "127.0.0.1", NULL, NULL, NULL, "This is comment #2"),
("test", 1, 3, 1271493064, 0, 1, "dummy", "39.39.39.39", NULL, NULL, NULL, "Another comment."),
("test", 1, 4, 1171892064, 0, 1, "dummy", "1.1.1.1", NULL, NULL, NULL, "This board sucks, lol."),
("test", 1, 5, 1471813064, 0, 1, "dummy", "2.2.2.2", NULL, NULL, NULL, "dickbutt");
