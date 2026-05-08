sudo install -m 755 flash-rp2040.sh /usr/local/bin/flash-rp2040.sh
sudo install -m 644 99-rp2040-flash.rules /etc/udev/rules.d/
sudo install -m 644 rp2040-flash@.service /etc/systemd/system/
sudo mkdir -p /opt/rp2040-firmware
sudo cp your-build.uf2 /opt/rp2040-firmware/firmware.uf2

sudo systemctl daemon-reload
sudo udevadm control --reload-rules
sudo udevadm trigger
