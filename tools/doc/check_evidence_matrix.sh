#!/usr/bin/env bash
#
# Checks that every requirement in the readiness checklist has a row in the
# requirement-to-evidence matrix.
#
# DOC-004 asks that every requirement map to implementation and tests. That
# is a property of two documents staying in step, which is exactly the kind
# of claim that rots silently: a requirement added to the checklist and never
# given a matrix row looks fine in both files read separately. So it is
# checked rather than asserted.
#
# The matrix names requirements in several shapes, all of which appear today
# and all of which are legitimate:
#
#   HYP-001–015          en-dash range
#   IRQ-001..008         dot range
#   TST-001/002/004      slash list, shared prefix
#   PRD-010..012 / DOC-014   combinations of the above
#
# Rows under `## Batch NNNN` headings are historical snapshots that the
# matrix's own header declares superseded, so only rows above the first such
# heading count as current coverage.
set -u

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
checklist="$repo_root/docs/readiness/PRODUCTION_READINESS_CHECKLIST.md"
matrix="$repo_root/docs/readiness/REQUIREMENT_EVIDENCE_MATRIX.md"

for file in "$checklist" "$matrix"; do
    if [ ! -f "$file" ]; then
        echo "missing: $file" >&2
        exit 1
    fi
done

perl - "$checklist" "$matrix" <<'PERL'
use strict;
use warnings;

my ($checklist, $matrix) = @ARGV;

# Requirements, in checklist order, with their status marker.
my @required;
open(my $cl, '<', $checklist) or die $!;
while (my $line = <$cl>) {
    next unless $line =~ /^- \[(.)\] \*\*([A-Z]+-[A-Z0-9]+)\*\*/;
    push @required, { id => $2, mark => $1 };
}
close $cl;

# Coverage, from current rows only. Stop at the first historical batch
# heading; the matrix header declares everything below it superseded.
my %covered;
open(my $mx, '<', $matrix) or die $!;
while (my $line = <$mx>) {
    last if $line =~ /^##\s+Batch\s/;
    next unless $line =~ /^\|\s*([^|]+?)\s*\|/;
    my $cell = $1;
    next if $cell =~ /^Requirement$/ or $cell =~ /^-+$/;

    # Expand every range and list shape found in the first cell.
    for my $token (split m{\s*/\s*|\s*,\s*}, $cell) {
        $token =~ s/`//g;
        $token =~ s/^\s+|\s+$//g;
        next unless length $token;

        if ($token =~ /^([A-Z]+)-(\d+)\s*(?:\.\.|–|-{2,}|—)\s*(\d+)$/) {
            my ($prefix, $from, $to) = ($1, $2, $3);
            my $width = length $2;
            for my $n ($from .. $to) {
                $covered{sprintf("%s-%0*d", $prefix, $width, $n)} = 1;
            }
        } elsif ($token =~ /^([A-Z]+-[A-Z0-9]+)$/) {
            $covered{$1} = 1;
        }
        # Bare numbers continuing a slash list are handled below, where the
        # shared prefix is still in scope.
    }
    # Slash lists with a shared prefix: TST-001/002/004/007 -- the prefix
    # appears once and later elements are bare numbers.
    if ($cell =~ /([A-Z]+)-(\d+)((?:\/\d+)+)/) {
        my ($prefix, $first, $rest) = ($1, $2, $3);
        my $width = length $first;
        $covered{sprintf("%s-%0*d", $prefix, $width, $first)} = 1;
        for my $n (split m{/}, $rest) {
            next unless length $n;
            $covered{sprintf("%s-%0*d", $prefix, $width, $n)} = 1;
        }
    }
}
close $mx;

my @unmapped = grep { !$covered{$_->{id}} } @required;

printf "\n== requirement-to-evidence coverage ==\n";
printf "  requirements in checklist : %d\n", scalar @required;
printf "  mapped in current matrix  : %d\n", scalar(@required) - scalar(@unmapped);
printf "  unmapped                  : %d\n", scalar @unmapped;

if (@unmapped) {
    # Report complete-but-unmapped separately: a requirement claimed done
    # with no evidence row is the case DOC-004 actually exists to prevent.
    my @done   = grep { $_->{mark} eq 'x' } @unmapped;
    my @open   = grep { $_->{mark} ne 'x' } @unmapped;
    if (@done) {
        printf "\n  COMPLETE but no evidence row (%d):\n", scalar @done;
        printf "    %s\n", $_->{id} for @done;
    }
    if (@open) {
        printf "\n  open and no evidence row (%d):\n", scalar @open;
        printf "    %s\n", join(' ', map { $_->{id} } @open);
    }
    # A ratchet, not a wall. The complete-but-unmapped debt predates this
    # check and DOC-004 tracks paying it down; failing outright would keep
    # `make doc-check` permanently red and so enforce nothing at all. What
    # must not happen is the gap WIDENING -- a requirement marked complete
    # with no evidence row is exactly what DOC-004 exists to prevent -- so
    # the baseline is the lowest count yet achieved, and
    # exceeding it fails. Lower it whenever rows are added; never raise it.
    my $baseline = 9;
    my $done_count = scalar @done;
    if ($done_count > $baseline) {
        printf "\n  evidence-matrix: FAIL -- %d complete requirements lack an\n", $done_count;
        printf "  evidence row, above the recorded baseline of %d. Add rows for\n", $baseline;
        printf "  the new ones rather than raising the baseline.\n";
        exit 1;
    }
    if ($done_count < $baseline) {
        printf "\n  evidence-matrix: PASS (%d complete-but-unmapped, below the\n", $done_count;
        printf "  baseline of %d -- lower the baseline in this script to lock it in)\n", $baseline;
        exit 0;
    }
    printf "\n  evidence-matrix: PASS (at the %d baseline; DOC-004 tracks closing it)\n",
        $baseline;
    exit 0;
}

printf "\n  evidence-matrix: PASS\n";
exit 0;
PERL
