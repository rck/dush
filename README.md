% DUSH(1) Dush User Manual
% Roland Kammerer
% July 10, 2012

# NAME

dush - find and display largest files/directories

# SYNOPSIS

dush [*OPTION*]... [*PATH*]

# DESCRIPTION

Dush recursively walks a given directory and generates stats according to the
size of contained files and/or directories.

If no *PATH* is specified, dush starts at '.'.

# OPTIONS

-b|k|m|g 
:   Output size in bytes, KB, MB or GB.

-n *NUM*
:   Display *NUM* largest files.

-v 
:   Print an ASCII graph showing how large files are compared to the largest one.

-d 
:   Include directories in the statistic. The size of a directory is the sum of
    the regular files in that directory (no subdirectories).

-f
:   Print full path of a file. Without it, display only the basename

-c
:   Display a count while walking the directory tree (slows down output a bit).

-l
:   Output only the file names, nothing else. This can be used to pipe the output
    to other programs (e.g., xargs rm -rf). Basically it enables *-f*, and disables
    all other gimmicks.

# Contributions
Benedikt Huber

