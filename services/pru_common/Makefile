SERVICE         := pru_common
INSTALLNONEXEC  := install -m 644
INSTALLDIR      := install -d -m 755

install:
	@$(INSTALLDIR) $(DESTDIR)/etc/modules-load.d/
	@$(INSTALLNONEXEC) 00_pru_rpmsg_modules.conf $(DESTDIR)/etc/modules-load.d/
	@echo "pru_common (shared PRU/RPMsg modules) Install Complete"

uninstall:
	@$(RM) $(DESTDIR)/etc/modules-load.d/00_pru_rpmsg_modules.conf
	@echo "pru_common Uninstall Complete"

clean:
	@echo "pru_common Clean Complete"
