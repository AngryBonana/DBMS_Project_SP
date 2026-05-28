CREATE DATABASE async_demo;
USE async_demo;
CREATE TABLE items (id INT NOT_NULL INDEXED, title STRING NOT_NULL);
INSERT INTO items (id, title) VALUES (1, 'first'), (2, 'second');
SELECT * FROM items;
