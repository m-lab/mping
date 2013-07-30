#!/bin/sh

a=0
p="plot"

echo "set term png" > temp.gnuplot
echo "set output \"output.png\"" >> temp.gnuplot
echo "set xlabel 'Number of packet in flight'" >> temp.gnuplot
echo "set ylabel 'Packets per second'" >> temp.gnuplot
echo "set key top left" >> temp.gnuplot

if [ "$#" -gt "$a" ]
then
  for var in "$@"
  do
    p="${p} \"$var\" using 1:2 t '$var out' with linespoints, \"$var\" using 1:3 t '$var in' with linespoints,"
  done
else
  for var in `ls size_*.temp`
  do
    p="${p} \"$var\" using 1:2 t '$var out' with linespoints, \"$var\" using 1:3 t '$var in' with linespoints,"
  done
fi 

echo "${p%?}" >> temp.gnuplot

gnuplot temp.gnuplot
eog output.png
