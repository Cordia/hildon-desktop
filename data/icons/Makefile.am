SUBDIRS=48x48

themedir = ${datadir}/hildon-desktop/icons/hicolor
theme_DATA = index.theme
gtk_update_icon_cache = gtk-update-icon-cache -f -t ${themedir}

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

