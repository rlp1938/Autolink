all:
	cc -Wall autolink.c -o autolink
	cc -Wall undangle.c -o undangle
debug:
	cc -g autolink.c -o autolink
	cc -g undangle.c -o undangle


