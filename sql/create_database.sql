CREATE TABLE comments (
	id		NUMERIC(20,0)	PRIMARY KEY,
	time	NUMERIC(20,0)	NOT NULL,
	ip		VARCHAR(256)	NOT NULL,
	text	VARCHAR(2000)	NOT NULL
);

INSERT INTO comments VALUES
(1, 1471893064, "192.168.1.1", "This is a sample comment!"),
(2, 1371293064, "127.0.0.1", "This is comment #2"),
(3, 1271493064, "39.39.39.39", "Another comment."),
(4, 1171892064, "1.1.1.1", "This board sucks, lol."),
(5, 1471813064, "2.2.2.2", "dickbutt");

