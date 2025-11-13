#!/bin/bash

SERVICE_NAME="nvtuner"
SERVICE_TEMPLATE_FILE="/etc/systemd/system/${SERVICE_NAME}@.service"

echo "nvtuner pre-uninstall: cleaning up systemd service instances and template..."

INSTANCES=$(find /etc/systemd/system/multi-user.target.wants/ -name "${SERVICE_NAME}@*.service" -print | xargs -n 1 basename)
if [ -n "$INSTANCES" ]; then
    echo "Disabling all active instances..."
    for instance in $INSTANCES; do
        UNIT_NAME="${instance%.service}"
        echo "  ${UNIT_NAME}"
        systemctl disable --now "${UNIT_NAME}" >/dev/null 2>&1 || :
    done
fi

echo "Removing service template file: ${SERVICE_TEMPLATE_FILE}"
rm -f "$SERVICE_TEMPLATE_FILE"

echo "Reloading systemd daemon..."
systemctl daemon-reload >/dev/null 2>&1 || :

echo "done."

exit 0
