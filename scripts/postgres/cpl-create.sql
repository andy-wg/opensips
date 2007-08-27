INSERT INTO version (table_name, table_version) values ('cpl','1');
CREATE TABLE cpl (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    cpl_xml TEXT,
    cpl_bin TEXT,
    CONSTRAINT ud_cpl UNIQUE (username, domain)
);

