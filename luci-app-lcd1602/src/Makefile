
APP_BINARY = lcd1602-daemon
APP_CFLAGS = -O3 -Wall
APP_LDFLAGS = -lm -luci

compile: lcd1602.c i2c1602.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_CFLAGS) $^ -o $(APP_BINARY) $(APP_LDFLAGS)

install: compile
	mkdir -p $(DESTDIR)/usr/bin
	cp $(APP_BINARY) $(DESTDIR)/usr/bin/$(APP_BINARY)

clean:
	rm -f $(APP_BINARY)
