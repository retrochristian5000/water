/*
 * FTP control-line tests for the IIS compatibility service.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include <string.h>
#include "wine/test.h"
#include "../ftp.h"

START_TEST(ftp)
{
    static const struct
    {
        const char *line;
        enum ftp_command command;
        const char *argument;
    } cases[] =
    {
        {"USER example\r\n", FTP_CMD_USER, "example"},
        {"pass \r\n", FTP_CMD_PASS, ""},
        {"PASS example\r\n", FTP_CMD_PASS, "example"},
        {"QUIT\r\n", FTP_CMD_QUIT, ""},
        {"NoOp\r\n", FTP_CMD_NOOP, ""},
        {"SYST\r\n", FTP_CMD_SYST, ""},
        {"TYPE I\r\n", FTP_CMD_TYPE, "I"},
        {"PASV\r\n", FTP_CMD_PASV, ""},
        {"LIST\r\n", FTP_CMD_LIST, ""},
        {"RETR a file.txt\r\n", FTP_CMD_RETR, "a file.txt"},
        {"STOR file.txt\r\n", FTP_CMD_STOR, "file.txt"},
        {"PWD\r\n", FTP_CMD_PWD, ""},
        {"CWD folder\r\n", FTP_CMD_CWD, "folder"},
        {"ABOR\r\n", FTP_CMD_ABOR, ""},
        {"EXAMPLE\r\n", FTP_CMD_UNKNOWN, NULL},
        {"USER\r\n", FTP_CMD_INVALID, NULL},
        {"USER \r\n", FTP_CMD_INVALID, NULL},
        {"PASS\r\n", FTP_CMD_INVALID, NULL},
        {"QUIT extra\r\n", FTP_CMD_INVALID, NULL},
        {"NOOP\n", FTP_CMD_INVALID, NULL},
        {"NOOP\r\nQUIT\r\n", FTP_CMD_INVALID, NULL},
        {" NOOP\r\n", FTP_CMD_INVALID, NULL},
        {"NOOP\t\r\n", FTP_CMD_INVALID, NULL},
        {"\r\n", FTP_CMD_INVALID, NULL},
    };
    struct ftp_control_line result;
    static const char embedded_nul[] = {'U','S','E','R',' ','a',0,'b','\r','\n'};
    char long_line[FTP_CONTROL_LINE_MAX + 1];
    unsigned int i;

    for(i = 0; i < ARRAY_SIZE(cases); ++i)
    {
        result = ftp_parse_control_line(cases[i].line, strlen(cases[i].line));
        ok(result.command == cases[i].command, "Case %u returned %d, expected %d.\n",
                i, result.command, cases[i].command);
        if(cases[i].argument && result.command == cases[i].command)
        {
            ok(result.argument_length == strlen(cases[i].argument), "Case %u argument length mismatch.\n", i);
            ok(!memcmp(cases[i].line + result.argument_offset, cases[i].argument,
                    strlen(cases[i].argument)), "Case %u argument mismatch.\n", i);
        }
    }
    result = ftp_parse_control_line(embedded_nul, sizeof(embedded_nul));
    ok(result.command == FTP_CMD_INVALID, "Embedded NUL accepted.\n");
    result = ftp_parse_control_line(NULL, 0);
    ok(result.command == FTP_CMD_INVALID, "NULL accepted.\n");

    memset(long_line, 'a', sizeof(long_line));
    memcpy(long_line, "USER ", 5);
    long_line[FTP_CONTROL_LINE_MAX - 2] = '\r';
    long_line[FTP_CONTROL_LINE_MAX - 1] = '\n';
    result = ftp_parse_control_line(long_line, FTP_CONTROL_LINE_MAX);
    ok(result.command == FTP_CMD_USER, "Maximum length command rejected.\n");
    long_line[FTP_CONTROL_LINE_MAX - 2] = 'a';
    long_line[FTP_CONTROL_LINE_MAX - 1] = '\r';
    long_line[FTP_CONTROL_LINE_MAX] = '\n';
    result = ftp_parse_control_line(long_line, sizeof(long_line));
    ok(result.command == FTP_CMD_INVALID, "Oversized command accepted.\n");
}
