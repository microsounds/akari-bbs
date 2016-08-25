CREATE TABLE comments (
	id		NUMERIC(20,0)	PRIMARY KEY,
	time	NUMERIC(20,0)	NOT NULL,
	ip		VARCHAR(16)		NOT NULL,
	text	VARCHAR(2000)	NOT NULL
);

INSERT INTO comments VALUES
(1, 1471893064, "This is a sample comment!"),
(2, 1371293064, "This is comment #2"),
(3, 1271493064, "Another comment."),
(4, 1171892064, "This board sucks, lol."),
(5, 1471813064, "dickbutt");


