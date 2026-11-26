/*
 * Private IIS FTP control-line validation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#ifndef WATER_INETINFO_FTP_H
#define WATER_INETINFO_FTP_H

#include <stddef.h>

#define FTP_CONTROL_LINE_MAX 512

enum ftp_command
{
    FTP_CMD_INVALID, FTP_CMD_UNKNOWN, FTP_CMD_USER, FTP_CMD_PASS,
    FTP_CMD_QUIT, FTP_CMD_NOOP, FTP_CMD_SYST, FTP_CMD_TYPE,
    FTP_CMD_PORT, FTP_CMD_PASV, FTP_CMD_LIST, FTP_CMD_RETR,
    FTP_CMD_STOR, FTP_CMD_PWD, FTP_CMD_CWD, FTP_CMD_ABOR
};

struct ftp_control_line
{
    enum ftp_command command;
    size_t argument_offset, argument_length;
};

static inline char ftp_upper(char c)
{
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

/* Inspect a single complete NVT-ASCII command, including CRLF.
 * No allocation, credential copying, logging, socket or filesystem access.
 * The caller retains ownership of the input. Telnet negotiation and
 * stream framing belong to a separate transport layer. */
static inline struct ftp_control_line ftp_parse_control_line(const char *line, size_t length)
{
    static const struct
    {
        char verb[5];
        enum ftp_command command;
        int argument; /* -1 forbidden, 0 optional, 1 required */
    } verbs[] =
    {
        {"USER", FTP_CMD_USER, 1}, {"PASS", FTP_CMD_PASS, 0},
        {"QUIT", FTP_CMD_QUIT, -1}, {"NOOP", FTP_CMD_NOOP, -1},
        {"SYST", FTP_CMD_SYST, -1}, {"TYPE", FTP_CMD_TYPE, 1},
        {"PORT", FTP_CMD_PORT, 1}, {"PASV", FTP_CMD_PASV, -1},
        {"LIST", FTP_CMD_LIST, 0}, {"RETR", FTP_CMD_RETR, 1},
        {"STOR", FTP_CMD_STOR, 1}, {"PWD", FTP_CMD_PWD, -1},
        {"CWD", FTP_CMD_CWD, 1}, {"ABOR", FTP_CMD_ABOR, -1}
    };
    struct ftp_control_line result = {FTP_CMD_INVALID, 0, 0};
    size_t i, j, end, verb_length;

    if(!line || length < 3 || length > FTP_CONTROL_LINE_MAX) return result;
    end = length - 2;
    if(line[end] != '\r' || line[end + 1] != '\n') return result;
    for(i = 0; i < end; ++i)
        if((unsigned char)line[i] < 0x20 || (unsigned char)line[i] > 0x7e)
            return result;

    for(verb_length = 0; verb_length < end && line[verb_length] != ' '; ++verb_length)
        if(ftp_upper(line[verb_length]) < 'A' || ftp_upper(line[verb_length]) > 'Z')
            return result;
    if(!verb_length) return result;

    for(i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i)
    {
        for(j = 0; j < verb_length && j < sizeof(verbs[i].verb) - 1; ++j)
            if(ftp_upper(line[j]) != verbs[i].verb[j]) break;
        if(j != verb_length || verbs[i].verb[j]) continue;
        if(verbs[i].argument < 0 && verb_length != end) return result;
        if(verbs[i].argument > 0 && verb_length + 1 >= end) return result;
        if(verbs[i].command == FTP_CMD_PASS && verb_length == end) return result;
        result.command = verbs[i].command;
        result.argument_offset = verb_length < end ? verb_length + 1 : end;
        result.argument_length = end - result.argument_offset;
        return result;
    }
    result.command = FTP_CMD_UNKNOWN;
    return result;
}
#endif
