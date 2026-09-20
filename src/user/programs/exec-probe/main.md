# exec probe

The image `fork-probe` execs into. Its whole job is to be a *different*
program that can prove it is running: it reports the argv it was given, so
the parent can distinguish a successful image replacement from a child that
merely continued.
