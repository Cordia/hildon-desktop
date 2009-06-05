SUBDIRS = mb tidy util launcher home $(A11Y_DIR)

INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS =	$(top_builddir)/src/mb/libmb.la			\
		$(MB2_STATIC_LIB)                               \
		$(top_builddir)/src/launcher/liblauncher.la 	\
		$(top_builddir)/src/home/libhome.la		\
		$(top_builddir)/src/util/libutil.la

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = main.c hildon-desktop.h

hildon_desktop_LDADD = $(MY_LIBS) $(HD_LIBS) $(A11Y_LA)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

schemadir = ${datadir}/gconf/schemas
schema_DATA = \ 
	hildon-desktop-views.schemas

cuddir = ${sysconfdir}/osso-cud-scripts
cud_SCRIPTS = \
	hildon-desktop-cud.sh

rfsdir = ${sysconfdir}/osso-rfs-scripts
rfs_SCRIPTS = \
	hildon-desktop-rfs.sh

backupconfdir = ${sysconfdir}/osso-backup/applications
backupconf_DATA = \
	hildon-desktop.conf

backupprebackupdir = ${sysconfdir}/osso-backup/pre-backup.d/
backupprebackup_SCRIPTS = \
	hildon-desktop.prebackup.sh

CLEANFILES = *~
