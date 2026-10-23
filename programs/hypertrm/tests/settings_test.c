/* Host-runnable narrow regression for the serial terminal input validator. */
#include "../settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct serial_settings s;
    const char *invalid_ports[] = {"", "COM", "COM0", "COM01", "LPT1", "COM257", "COM999999999", "COM1:", " COM1", "COM1x"};
    const char *invalid_bauds[] = {"", "0", "-1", "38400x", "4000001", "999999999999999999999", " 9600"};
    unsigned i;

    assert(serial_parse_settings("COM1", "9600", &s));
    assert(strcmp(s.device, "\\\\.\\COM1") == 0);
    assert(s.baud == 9600);
    assert(serial_parse_settings("cOm256", "115200", &s));
    assert(strcmp(s.device, "\\\\.\\COM256") == 0);
    for (i = 0; i < sizeof(invalid_ports) / sizeof(invalid_ports[0]); i++)
        assert(!serial_parse_settings(invalid_ports[i], "9600", &s));
    for (i = 0; i < sizeof(invalid_bauds) / sizeof(invalid_bauds[0]); i++)
        assert(!serial_parse_settings("COM1", invalid_bauds[i], &s));
    assert(!serial_parse_settings(NULL, "9600", &s));
    assert(!serial_parse_settings("COM1", "9600", NULL));
    puts("serial settings: all tests passed");
    return 0;
}
