all: httpd

LIBS = -lpthread

httpd: httpd.c
	gcc httpd.c -o httpd $(LIBS)

clean:
	rm httpd