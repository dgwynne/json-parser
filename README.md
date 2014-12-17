JSON Parser
===========

This is a parser for JSON messages written in C. It is largely
modelled on Joyent's http-parser.

It is intended to parse a stream of JSON data. It does not make
syscalls, it does not buffer data, and has very minimal allocation
overhead.

As JSON data becomes available to the application, it is fed to an
instance of a json_parser which then throws events (eg, on_string,
on_array_start, on_separator) as it interprets the payload. It does
not buffer data though, so multiple events may be thrown for the
same value.

Unlike other JSON parsing libraries, it does not turn JSON numbers
into numeric types (ints, doubles, etc). It presents the numbers
as strings for the application to interpret as it sees fit. The
JSON spec says a number is a particular series of characters, but
makes no claims about limits on precision or size of the number.
Presenting the number as a string to the application allows the
application to choose not to take the overhead of interpreting
numbers, or to conditionally interpret them, or use another library
to support arbitrary precision numbers.

Why?
----

I'm an idiot.
