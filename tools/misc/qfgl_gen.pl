#! /usr/bin/perl -w
use strict;

&main(@ARGV);

sub main () {
	my ($input_file, $funcs_file, $header_file, $footer_file) = @_;
	my (@input, @funcs, $funcs, @header, @footer, $line);
	
	if ($#ARGV != 3) {
		die("Usage: $0 <input header> <funcs file> <header> <footer>\n");
	}

	@input = &read_file(sprintf("indent -l1000 -nhnl -st %s |", $input_file));
	@funcs = &read_file($funcs_file);
	@header = &read_file($header_file);
	@footer = &read_file($footer_file);

	if ($#funcs == -1) {
		die("Use a function or two!\n");
	}
	$funcs = join ("|", @funcs);

	foreach $line (@header) {
		print("$line\n");
	}

	&print_funcs ($funcs, \@input);

	foreach $line (@footer) {
		print("$line\n");
	}
}

sub print_funcs () {
	my (%names, @names, $name);
	my ($return, $func, $args, $funcs, $line, @input);

	$funcs = shift;
	@input = @{+shift};

	foreach $line (@input) {

		if ($line =~ /GLAPI\s+(.+?)\s*GLAPIENTRY\s+([^\s\(]+)\s*\(([^()]+)\)/) {
			$return = $1; $name = $2, $args = $3;
			$names{$name} = [$return, $args];
		}
	}
	@names = sort { $a cmp $b } keys %names;
	foreach $name (@names) {
		($return, $args) = @{$names{$name}};
		if ($name =~ /(glX|ARB|EXT|MESA|NV|SGIS|SGIX|SGI|APPLE|HP|IBM|INTEL|SUN|SUNX|INGR|ATI|WIN|PGI)/) {
			# It is an extension, ignore it.
		} elsif ($name =~ /($funcs)/) {
			print("QFGL_NEED ($return, $name, ($args));\n");
		} else {
			print("QFGL_DONT_NEED ($return, $name, ($args));\n");
		}
	}
}

sub read_file () {
	my ($file, @lines);

	$file = shift;

	open (FILE, $file) or die "Can't open file $file: $!\n";;

	while (<FILE>) {
		chomp($_);
		push(@lines, $_);
	}

	close (FILE);

	return @lines;
}
