#! /usr/bin/perl

use strict;

my $emu_bin = '../cxnes';
my $emu_options = '-o blargg_test_rom_hack_enabled=true -o default_overclock_mode=disabled --regression-test';
my $romdir = 'nes_test_roms';

# FIXME commandline options
# FIXME handle crashes, errors
# FIXME handle hangs with SIGALRM

my @roms;
my @screenshot_roms;

while (<>) {
	chomp;
	my @fields = split /\s+/, $_;
	if (@fields == 3) {
		push @screenshot_roms, \@fields;
	} else {
		push @roms, $fields[0];
	}
}

sub run_test
{
	my ($rom) = $_[0];
	my @output;

	print "$rom...";
	open EMU, "$emu_bin $emu_options $romdir/$rom 2>/dev/null |" ||
	  die "Couldn't run emu: $!\n";
	while (<EMU>) {
		next unless (/^BLARGG:/);
		# Bisquit's test roms have colored output. Pretty,
		# but I don't want colors in the test log.
		s/\x1b\[\d+;\d+m//g;
		push @output, $_;
	}
	close EMU;

	if ($output[0] =~ /TEST FINISHED \(00\)/) {
		print "\tPass\n";
	} else {
		print "\tFail (output below)\n";
		print @output;
	}
	    
}

sub run_screenshot_test
{
	my ($rom, $image, $frames) = @_;

	print "$rom...";
	system("$emu_bin $emu_options --frame-dumpfile=/tmp/dumpfile.png " .
                  "--test-duration=$frames $romdir/$rom > /dev/null 2>&1");

	if ($? == -1) {
		print "failed to execute: $!\n";
	} elsif ($? & 127) {
		printf "child died with signal %d, %s coredump\n",
                        ($? & 127),  ($? & 128) ? 'with' : 'without';
	} elsif ($?) {
		printf "child exited with value %d\n", $? >> 8;
	}

	my $orig_sum=`identify -quiet -format "%#" $romdir/$image`;
	my $new_sum=`identify -quiet -format "%#" /tmp/dumpfile.png`;

	if ($orig_sum ne $new_sum) {
		print "\tFail\n";
	} else {
		print "\tPass\n";
	}
}

run_test($_) for @roms;
run_screenshot_test(@$_) for @screenshot_roms;
