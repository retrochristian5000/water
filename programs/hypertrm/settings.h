/* Win32 COM path parser for Water's experimental serial terminal.
 * Pure C so port-name and baud validation can be tested without a Windows VM.
 */
#ifndef WATER_HYPERTRM_SETTINGS_H
#define WATER_HYPERTRM_SETTINGS_H

struct serial_settings {
    char device[16]; /* Canonical \\.\COMn form, including NUL */
    unsigned long baud;
};

/* Returns zero on invalid input; never opens a device. */
int serial_parse_settings(const char *port, const char *baud, struct serial_settings *out);

#endif
