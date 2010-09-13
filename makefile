CFLAGS = `pkg-config --cflags glib-2.0 purple`
LIBS =  `pkg-config --libs glib-2.0 purple`

all: purple-chatroom-bridge.so

purple-chatroom-bridge.so : purple-chatroom-bridge.c
	gcc -o $@ -shared -fPIC $(CFLAGS) $< $(LIBS)

install :
	cp purple-chatroom-bridge.so .purple/plugins/

run :
	pidgin -m --config=.purple/
