# Converts the bytes a of disk image into a C array

IMG_INPUT = '~/floppy.img'
SRC_OUTPUT = 'floppy_array.c'

infile = File.open( IMG_INPUT, 'rb' )
outfile = File.open( SRC_OUTPUT, 'w' )

infile.readline until infile.eof?
outfile.print "unsigned char floppy_array[#{infile.pos}] = {\n  "

number = 0

infile.rewind

until infile.eof? do
  outfile.printf "0x%x,", infile.readchar
  number += 1

  if number == 10
    outfile.print "\n  "
    number = 0
  end
end

infile.close

outfile.print "\n};\n"
outfile.close
