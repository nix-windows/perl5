#!/usr/bin/perl

use strict;
use warnings FATAL => 'all';

use Config;
use Test::More;
use ExtUtils::PL2Bat;
use Cwd qw/cwd/;

plan($^O eq 'MSWin32' ? (tests => 7) : skip_all => 'Only usable on Windows');

my $filename = 'test_exec';
my @files;

open my $out, '>', $filename or die "Couldn't create $filename: $!";
print $out "#! perl -w\nexit \$ARGV[0];\n";
close $out;

pl2bat(in => $filename);

my $tmp_path = do { # keep PATH from going above 1023 chars (incompatible on win2k)
	my $perl_path = $^X;
	my $cmd_path = `where cmd`;
	$_ =~ s/[\\\/][^\\\/]+$// for $perl_path, $cmd_path;
	join $Config{path_sep}, cwd(), $perl_path, $cmd_path;
};
print STDERR "make_executable.t $^X";
print STDERR "make_executable.t $tmp_path";

foreach my $i (42, 51, 0) {
	local $ENV{PATH} = $tmp_path;
	print STDERR "make_executable.t $ENV{PATH}";
	my $ret = system $filename, $i;
	is $ret & 0xff, 0, 'test_exec executed successfully';
	is $ret >> 8, $i, "test_exec $i return value ok";
}

push @files, grep { -f } map { $filename.$_ } split / $Config{path_sep} /x, $ENV{PATHEXT} || '';
is scalar(@files), 1, "Executable file exists";

unlink $filename, @files;
