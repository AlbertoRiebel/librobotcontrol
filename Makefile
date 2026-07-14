prefix          := /usr
RM              := rm -r -f
INSTALL         := install -m 755
INSTALLDIR      := install -d -m 755
INSTALLDIRWRITE := install -d -m 777
INSTALLNONEXEC  := install -m 644
all:
	@make -C library --no-print-directory
	@make -C examples --no-print-directory
	@make -C services/rc_battery_monitor --no-print-directory
	@make -C services/robotcontrol --no-print-directory
	@make -C services/pru_common --no-print-directory
	@make -C services/pru_servo --no-print-directory
	@make -C services/pru_encoder --no-print-directory
install:
	@$(INSTALLDIR) $(DESTDIR)$(prefix)/share/robotcontrol
	@cp -r -f  rc_project_template $(DESTDIR)$(prefix)/share/robotcontrol/
	@$(INSTALLDIR) $(DESTDIR)$(prefix)/bin
	@$(INSTALL) device_tree/configure_robotics_dt.sh $(DESTDIR)$(prefix)/bin/configure_robotics_dt
	@if [ -f device_tree/dtb-5.10-ti/am335x-boneblue.dtb ]; then \
		install -m 644 device_tree/dtb-5.10-ti/am335x-boneblue.dtb /boot/dtbs/5.10.168-ti-r83/am335x-boneblue.dtb; \
		echo "DTB instalado: /boot/dtbs/5.10.168-ti-r83/am335x-boneblue.dtb"; \
	fi
	@make -C pru_firmware -s install
	@make -C library -s install
	@make -C examples -s install
	@make -C services/rc_battery_monitor -s install
	@make -C services/robotcontrol -s install
	@make -C services/pru_common -s install
	@make -C services/pru_servo -s install
	@make -C services/pru_encoder -s install
clean:
	@make -C pru_firmware -s clean
	@make -C library -s clean
	@make -C examples -s clean
	@make -C services/rc_battery_monitor -s clean
	@make -C services/robotcontrol -s clean
	@make -C services/pru_common -s clean
	@make -C services/pru_servo -s clean
	@make -C services/pru_encoder -s clean
	@make -C rc_project_template -s clean
	@$(RM) debian/librobotcontrol
	@$(RM) debian/librobotcontrol.postrm.debhelper
	@$(RM) debian/librobotcontrol.substvars
	@$(RM) debian/files
	@$(RM) debian/*.debhelper.log
	@$(RM) debian/debhelper-build-stamp
	@$(RM) docs/html
	@echo "All Directories Cleaned"
uninstall:
	@make -C pru_firmware -s uninstall
	@make -C library -s uninstall
	@make -C examples -s uninstall
	@make -C services/rc_battery_monitor -s uninstall
	@make -C services/robotcontrol -s uninstall
	@make -C services/pru_common -s uninstall
	@make -C services/pru_servo -s uninstall
	@make -C services/pru_encoder -s uninstall
	@$(RM) $(DESTDIR)$(prefix)/bin/configure_robotics_dt
	@$(RM) $(DESTDIR)$(prefix)/share/robotcontrol
	@$(RM) $(DESTDIR)/var/lib/robotcontrol
	@$(RM) $(DESTDIR)/var/log/robotcontrol
	@echo "Robotics Cape Uninstalled"
package:
	debuild -us -uc
# no compile option for package
packagenc:
	debuild -us -uc -nc
