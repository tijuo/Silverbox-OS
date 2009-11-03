#!/usr/bin/perl

if( $#ARGV != 0 ) # only one argument accepted
{
  printf("Missing filename\n");
  exit(1);
}

$fname = shift;

open(fh, $fname);
$sym = 0;

while(<fh>)
{
  if( $_ =~ /^\n$/ )
  {
    $sym = 0;
    break;
  }
  if( $sym == 1 )
  {
    @fields = split(' ', $_);

    if( $fields[-3] ne '*ABS*' )
    {
      print "$fields[0] $fields[-1]\n";
    }
  }
  if( $_ =~ /SYMBOL TABLE/ )
  {
    $sym = 1;
  }
}

close(fh);
