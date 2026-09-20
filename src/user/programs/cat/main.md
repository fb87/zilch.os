# cat

Streams files named on the command line, or standard input when given none,
to standard output in 512-byte chunks.

The no-argument form is what makes it usable as a pipeline consumer, and the
argument form is the ordinary file read. Both share one loop that treats a
short read as data and a zero-length read as end of file.
