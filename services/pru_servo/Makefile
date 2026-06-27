SERVICE         := pru_servo
INSTALLNONEXEC  := install -m 644
INSTALLDIR      := install -d -m 755
DTB_SRC         := ../../../device_tree/dtb-5.10-ti/am335x-boneblue.dtb
DTB_DEST        := /boot/dtbs/5.10.168-ti-r83/am335x-boneblue.dtb

install:
	@$(INSTALLDIR) $(DESTDIR)/lib/systemd/system
	@$(INSTALLNONEXEC) $(SERVICE).service $(DESTDIR)/lib/systemd/system/
	@$(INSTALLDIR) $(DESTDIR)/etc/modules-load.d/
	@$(INSTALLNONEXEC) 00_pru_servo_modules.conf $(DESTDIR)/etc/modules-load.d/
	@if [ -f $(DTB_SRC) ]; then \
		install -m 644 $(DTB_SRC) $(DTB_DEST); \
		echo "DTB instalado: $(DTB_DEST)"; \
	fi
	@systemctl enable $(SERVICE).service 2>/dev/null || true
	@echo "pru_servo Service Install Complete"

uninstall:
	@systemctl disable $(SERVICE).service 2>/dev/null || true
	@$(RM) $(DESTDIR)/lib/systemd/system/$(SERVICE).service
	@$(RM) $(DESTDIR)/etc/modules-load.d/00_pru_servo_modules.conf
	@echo "pru_servo Service Uninstall Complete"

clean:
	@echo "pru_servo Service Clean Complete"
