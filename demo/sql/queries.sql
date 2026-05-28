SELECT id, name, city FROM users WHERE id == 2;
UPDATE users SET city = 'Grodno' WHERE id == 2;
SELECT * FROM users;
SELECT * FROM logs WHERE id BETWEEN 10 AND 12;
SELECT * FROM logs WHERE msg LIKE 'alpha.*';
SELECT * FROM logs WHERE id >= 11;
