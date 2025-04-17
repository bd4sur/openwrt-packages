all: lcd1602.c i2c1602.c
	$(CC) -O3 -Wall $^ -o lcd1602 -lm

.PHONY: clean
clean:
	rm -f lcd1602
