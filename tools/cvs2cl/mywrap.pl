#!/usr/bin/perl -w

use Text::Wrap;

my $test_text =
"Change from Melissa O'Neill <oneill at cs.sfu.ca>:

Removed some superfluous tests from conditionals (it's true that they
make it clear what is true at that point, but that could be expressed
in a comment, rather than in executed code).  (Unfortunately, this
meant that I outdented a fairly large chunk of code, making the diff
look like a more extensive change than it really is.)

   This paragraph has been entirely indented.  It consists of six
   lines, each of which is indented by no less than three spaces.
   This paragraph has been entirely indented.  It consists of six
   lines, each of which is indented by no less than three spaces.
   This paragraph has been entirely indented.  It consists of six
   lines, each of which is indented by no less than three spaces.

This script demonstrates a bug in Text::Wrap.  The very long line of
equal signs that ought to come right after this paragraph will be
wrongly relocated:

====================================================================

See?  When the bug happens, we'll get the line of equal signs below,
even though it should be above.";


### My reimplementation of Text::Wrap().


sub my_wrap ()
{
  my $initial_indent    = shift;
  my $subsequent_indent = shift;
  my $text              = shift;


  $text =~ s/\n/ /g;

  
  
}





# Print out the test text with no wrapping:
print "$test_text";
print "\n";
print "\n";

# Now print it out wrapped, and see the bug:
print wrap ("\t", "        ", "$test_text");
print "\n";
print "\n";


__END__

If the line of equal signs were one shorter, then the bug doesn't
happen.  Interesting.

Anyway, rather than fix this in Text::Wrap, we might as well write a
new wrap() which has the following much-needed features:

* initial indentation, like current Text::Wrap()
* subsequent line indentation, like current Text::Wrap()
* user chooses among: force-break long words, leave them alone, or die()?
* preserve existing indentation: chopped chunks from an indented line
  are indented by same (like this line, not counting the asterisk!)
* optional list of things to preserve on line starts, default ">"

Note that the last two are essentially the same concept, so unify in
implementation and give a good interface to controlling them.

And how about:

Optionally, when encounter a line pre-indented by same as previous
line, then strip the newline and refill, but indent by the same.
Yeah...
