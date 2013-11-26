% DUSH(1) Dush User Manual
% Roland Kammerer
% August 20, 2013

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

-n *NUM*, \--num *NUM*
:   Display *NUM* largest files.

-v, \--graph 
:   Print an ASCII graph showing how large files are compared to the largest one.

-d, \--dirs 
:   Include directories in the statistic. The size of a directory is the sum of
    the regular files in that directory (no subdirectories).

-s, \--subdirs 
:   Like *-d*, but also include the size of subdirectories. This option also enables *-d*

-e *NAME*, \--exclude *NAME*
:   Exclude directories with the given *NAME* (e.g., dush -e .git -e .svn ~)

-f, \--full
:   Print full path of a file. Without it, display only the basename.

-c, \--count
:   Display a count while walking the directory tree (slows down output a bit).

-t, \--table
:   Filter output with **column -t**

-l, \--list
:   Output only the file names, nothing else. This can be used to pipe the output
    to other programs (e.g., xargs rm -rf). Basically it enables *-f*, and disables
    all other gimmicks.

-h, \--help
:   Display this help and exit.

# NOTES

If you compare the size with **du**(1), make sure to use **--apparent-size**

# CONTRIBUTORS
Benedikt Huber.

