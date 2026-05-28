CREATE DATABASE demo;
USE demo;
CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING NOT_NULL, city STRING);
CREATE TABLE logs (id INT NOT_NULL INDEXED, msg STRING NOT_NULL);
INSERT INTO users (id, name, city) VALUES (1, 'Ann', 'Minsk'), (2, 'Bob', 'Brest');
INSERT INTO logs (id, msg) VALUES (10, 'alpha'), (11, 'beta'), (12, 'alphabet');
