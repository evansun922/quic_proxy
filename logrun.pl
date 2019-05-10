#!/usr/bin/perl
use strict;
use warnings;

close STDIN;
close STDOUT;
close STDERR;

die if ($#ARGV < 0); my $logdir = shift(@ARGV);
die if ($#ARGV < 0); my $program = join(' ', @ARGV);
my $stdout_execute = "/opt/scm/rotatelogs $logdir/stdout_%Y%m%d.log 86400 +480";
my $stderr_execute = "/opt/scm/rotatelogs $logdir/stderr_%Y%m%d.log 86400 +480";

pipe *PI1, *PO1 || die "Cannot open pipe, $!\n";
pipe *PI2, *PO2 || die "Cannot open pipe, $!\n";
my $oldfd = select(PO1); $|=1; select(PO2); $|=1; select($oldfd);

my $pid1 = fork();
defined($pid1) || die "Cannot fork new process, $!\n";
if ($pid1 == 0) {
	close PO1;
	close PI2;
	close PO2;
	open(STDIN, "<&PI1") || die "Cannot redirect STDIN, $!\n";
	exec($stdout_execute) || die "Cannot execute program, $!\n";
	exit(0);
}

my $pid2 = fork();
defined($pid2) || die "Cannot fork new process, $!\n";
if ($pid2 == 0) {
	close PI1;
	close PO1;
	close PO2;
	open(STDIN, "<&PI2") || die "Cannot redirect STDIN, $!\n";
	exec($stderr_execute) || die "Cannot execute program, $!\n";
	exit(0);
}

close PI1; close PI2;

open(STDOUT, ">&PO1") || die "Cannot redirect STDOUT, $!\n";
open(STDERR, ">&PO2") || die "Cannot redirect STDERR, $!\n";
exec($program) || die "Cannot execute program, $!\n";
