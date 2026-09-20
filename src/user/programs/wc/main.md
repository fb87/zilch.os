# wc

Counts lines, words and bytes, for files named on the command line or for
standard input when given none.

Parses no flags at all. `wc -l` therefore treats `-l` as a filename, fails
to open it, and prints nothing — worth knowing before using it as a probe,
because that looks exactly like a broken pipeline. See readiness checklist
0156, where it misled an investigation.
