fuseflect2
==========

mirroring directory with logging option

it write log file of all filesystem modification to logfile
some replacement for inotify,
it reopen logfile for every record, so usage like that:

watch logfile, if it modified, mv it to queue, proceed the file. remove.


based on code from http://users.softlab.ntua.gr/~thkala/projects/fuseflect/fuseflect.html

