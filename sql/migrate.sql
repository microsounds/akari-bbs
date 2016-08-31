/* automated migration to version 3 */

CREATE TABLE new (
	id      NUMERIC(20,0)   PRIMARY KEY,
	time    NUMERIC(20,0)   NOT NULL,
	ip      VARCHAR(256)    NOT NULL,
	name    VARCHAR(500),
	trip    VARCHAR(500),
	text    VARCHAR(2000)   NOT NULL
);

INSERT INTO new(id, time, ip, text)
	SELECT id, time, ip, text FROM comments;
DROP TABLE comments;
ALTER TABLE new RENAME TO comments;
VACUUM;
