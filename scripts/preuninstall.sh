#!/bin/bash

SERVICE_NAME="nvtuner"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

echo "nvtuner pre-uninstall: removing systemd service..."

systemctl stop "${SERVICE_NAME}.service" >/dev/null 2>&1 || :
systemctl disable "${SERVICE_NAME}.service" >/dev/null 2>&1 || :
rm -f "$SERVICE_FILE"

systemctl daemon-reload >/dev/null 2>&1 || :

echo "done."

exit 0
