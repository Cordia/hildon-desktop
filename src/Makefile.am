SUBDIRS = mb tidy util launcher home $(A11Y_DIR)

INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS =	$(top_builddir)/src/mb/libmb.la			\
		$(MB2_STATIC_LIB)                               \
		$(top_builddir)/src/launcher/liblauncher.la 	\
		$(top_builddir)/src/home/libhome.la		\
		$(top_builddir)/src/util/libutil.la

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = main.c hildon-desktop.h

UNWANTED = -Wl,--export-dynamic -export-dynamic -rdynamic
hildon_desktop_LDADD = \
	$(filter-out $(UNWANTED),$(MY_LIBS) $(HD_LIBS) $(A11Y_LA))
hildon_desktop_LDFLAGS = -Wl,--version-script=export.map \
	$(filter-out $(UNWANTED),$(MAEMO_LAUNCHER_LIBS))
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline $(MAEMO_LAUNCHER_CFLAGS)

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

backuppostbackupdir = ${sysconfdir}/osso-backup/post-backup.d/
backuppostbackup_SCRIPTS = \
	hildon-desktop-post-backup.sh

backuprestoredir = ${sysconfdir}/osso-backup/restore.d/always/
backuprestore_SCRIPTS = \
	hildon-desktop-restore.sh

powervrconfdir = ${sysconfdir}/powervr.d
powervrconf_DATA = \
	hildon-desktop.ini

CLEANFILES = *~
