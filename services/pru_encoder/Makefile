SERVICE         := pru_encoder
INSTALLNONEXEC  := install -m 644
INSTALLDIR      := install -d -m 755

install:
	@$(INSTALLDIR) $(DESTDIR)/lib/systemd/system
	@$(INSTALLNONEXEC) $(SERVICE).service $(DESTDIR)/lib/systemd/system/
	@systemctl enable $(SERVICE).service 2>/dev/null || true
	@echo "pru_encoder Service Install Complete"

uninstall:
	@systemctl disable $(SERVICE).service 2>/dev/null || true
	@$(RM) $(DESTDIR)/lib/systemd/system/$(SERVICE).service
	@echo "pru_encoder Service Uninstall Complete"

clean:
	@echo "pru_encoder Service Clean Complete"
