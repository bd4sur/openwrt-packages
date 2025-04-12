all: nano_infer_ws_server.c nano_infer.c
	$(CC) -O3 -ffast-math -Wall $^ -o nano-infer-ws-server -lm -lwebsockets

.PHONY: clean
clean:
	rm -f nano-infer-ws-server
