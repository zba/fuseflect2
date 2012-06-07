#
# Regular cron jobs for the fuseflect2 package
#
0 4	* * *	root	[ -x /usr/bin/fuseflect2_maintenance ] && /usr/bin/fuseflect2_maintenance
