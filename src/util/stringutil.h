/*
**  This file is part of Vidalia, and is subject to the license terms in the
**  LICENSE file, found in the top level directory of this distribution. If you
**  did not receive the LICENSE file with this file, you may obtain it from the
**  Vidalia source package distributed by the Vidalia Project at
**  http://www.vidalia-project.net/. No part of Vidalia, including this file,
**  may be copied, modified, propagated, or distributed except according to the
**  terms described in the LICENSE file.
*/

/*
** \file stringutil.h
** \version $Id$
** \brief Common string manipulation functions
*/

#ifndef _STRINGUTIL_H
#define _STRINGUTIL_H

#include <QStringList>
#include <QHash>


/** Creates a QStringList from the array of C strings. */
QStringList char_array_to_stringlist(char **arr, int len);

/** Ensures all characters in str are in validChars. If a character appears
 * in str but not in validChars, it will be removed and the resulting
 * string returned. */
QString ensure_valid_chars(const QString &str, const QString &validChars);

/** Scrubs an email address by replacing "@" with " at " and "." with " dot ". */
QString scrub_email_addr(const QString &email);

/** Conditionally assigns errmsg to string if str is not null and returns
 * false. */
bool err(QString *str, const QString &errmsg);

/** Wraps <b>str</b> at <b>width</b> characters wide, using <b>sep</b> as the
 * word separator (" ", for example), and placing the line ending <b>le</b> at
 * the end of each line, except the last.*/
QString string_wrap(const QString &str, int width, 
                    const QString &sep, const QString &le);

/** Encodes the bytes in <b>buf</b> as an uppercase hexadecimal string and
 * returns the result. This function is derived from base16_encode() in Tor's
 * util.c. See LICENSE for details on Tor's license. */
QString base16_encode(const QByteArray &buf);

/** Given a string <b>str</b>, this function returns a quoted string with all
 * '"' and '\' characters escaped with a single '\'. */
QString string_escape(const QString &str);

/** Given a quoted string <b>str</b>, this function returns an unquoted,
 * unescaped string. <b>str</b> must start and end with an unescaped quote. */
QString string_unescape(const QString &str, bool *ok = 0);

/** Parses a series of space-separated key[=value|="value"] tokens from
 * <b>str</b> and returns the mappings in a QHash. If <b>str</b> was unable
 * to be parsed, <b>ok</b> is set to false. */
QHash<QString,QString> string_parse_keyvals(const QString &str, bool *ok = 0);

/** Returns true if <b>str</b> is a valid hexademical string. Returns false
 * otherwise. */
bool string_is_hex(const QString &str);

#endif

