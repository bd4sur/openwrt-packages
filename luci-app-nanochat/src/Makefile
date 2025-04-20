
NANO_SERVER_BINARY = nano-infer-ws-server
NANO_SERVER_CFLAGS = -O3 -ffast-math -Wall
NANO_SERVER_LDFLAGS = -lm -lwebsockets

compile: nano_infer.c nano_infer_ws_server.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NANO_SERVER_CFLAGS) $^ -o $(NANO_SERVER_BINARY) $(NANO_SERVER_LDFLAGS)

install: compile
	mkdir -p $(DESTDIR)/usr/bin
	cp $(NANO_SERVER_BINARY) $(DESTDIR)/usr/bin/$(NANO_SERVER_BINARY)

clean:
	rm -f $(NANO_SERVER_BINARY)
